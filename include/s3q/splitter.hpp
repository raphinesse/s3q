#pragma once

#include "bucket.hpp"
#include "util.hpp"

#include <ips4o/memory.hpp>
#include <ips4o/sequential.hpp>

#include <cassert>
#include <cstddef>
#include <limits>
#include <utility>

namespace s3q::detail {

/**
 * Encapsulates IPS4o-based in-place bucket splitting for S3Q.
 *
 * Analogous to Classifier<Cfg>: owns the IPS4o sorter state (BufferStorage +
 * LocalData), performs a single-level partition pass, and repairs underflowing
 * buckets.  BatchedPriorityQueue owns one instance; every Level holds a
 * reference to it.
 */
template <class Cfg>
class BucketSplitter {
public:
    using Buffer = typename Bucket<Cfg>::Buffer;

    // Comparator for IPS4o that orders Item objects by key.
    struct ItemKeyLess {
        bool operator()(const typename Cfg::Item& a,
                        const typename Cfg::Item& b) const noexcept {
            return Cfg::getKey(a) < Cfg::getKey(b);
        }
    };

    // IPS4o configuration for single-pass in-place bucket partitioning.
    // AllowEqualBuckets=false keeps bucket_start interpretation simple.
    // LogBuckets matches S3Q's existing classifier depth.
    // All numeric parameters reproduce the IPS4o defaults defined in
    // ips4o/config.hpp (IPS4OML_* macros), which are #undef'd at the end of
    // that header and are therefore unavailable here.
    using SplitCfg = ips4o::ExtendedConfig<
        typename Cfg::Item*,
        ItemKeyLess,
        ips4o::Config<
            false,                      // AllowEqualBuckets (IPS4o default: true)
            16,                         // BaseCaseSize      (IPS4OML_BASE_CASE_SIZE)
            16,                         // BaseCaseMultiplier(IPS4OML_BASE_CASE_MULTIPLIER)
            (2 << 10),                  // BlockSize: 2 KiB  (IPS4OML_BLOCK_SIZE)
            std::ptrdiff_t,             // BucketType        (IPS4o default)
            (4 << 10),                  // DataAlignment:4KiB(IPS4OML_DATA_ALIGNMENT)
            5,                          // EqualBuckTh       (IPS4OML_EQUAL_BUCKETS_THRESHOLD)
            Cfg::kLogMaxDegree + 1,     // LogBuckets — matches S3Q classifier
            4,                          // MinParBlksPerThread(IPS4OML_MIN_PARALLEL_BLOCKS_PER_THREAD)
            20,                         // OversamplingPct   (IPS4OML_OVERSAMPLING_FACTOR_PERCENT)
            7                           // UnrollClassifier  (IPS4OML_UNROLL_CLASSIFIER)
        >>;

    using SplitSorter = ips4o::detail::Sorter<SplitCfg>;
    using diff_t      = typename SplitSorter::diff_t;

    // Shared result type for all three split methods.
    struct SplitResult {
        int surviving;
        struct Range {
            diff_t         start, end;
            typename Cfg::Key sup;
        };
        Range  ranges[SplitCfg::kMaxBuckets]; // stack-allocated, no heap
        Buffer buf;                            // owns the partitioned data
    };

    BucketSplitter()
        : bufStorage_(1)
        , localDataPtr_(SplitCfg::kDataAlignment, ItemKeyLess{},
                        bufStorage_.get()) {}

    // PUBLIC — combines partition + repair; this is the only method Level
    // calls.
    // maxBuckets caps the number of surviving buckets after repair; any excess
    // buckets are merged from the tail (the merged result may overflow and will
    // be recursively split by fixOverflowingBuckets).
    SplitResult split(Buffer&& buf, typename Cfg::Key oldSup,
                      std::ptrdiff_t minBucketSize, int maxBuckets) {
        auto result = partition(std::move(buf), oldSup);
        repair(result, minBucketSize, maxBuckets);
        return result;
    }

    // Runs one IPS4o partition pass; populates result.ranges and result.buf.
    // Does NOT perform any underflow-merge repair.
    // Exposed as public to allow independent unit testing of IPS4o boundary
    // correctness.
    SplitResult partition(Buffer&& buf, typename Cfg::Key oldSup) {
        using Range = typename SplitResult::Range;

        SplitResult result;
        result.buf = std::move(buf);
        const auto n = ssize(result.buf);

        diff_t bucket_start[SplitCfg::kMaxBuckets + 1];
        SplitSorter sorter(localDataPtr_.get());
        auto partition_res =
            sorter.partitionOnce(result.buf.data(), result.buf.data() + n,
                                 bucket_start);
        const auto num_buckets = partition_res.first;

        // Read suprema BEFORE resetLocalData() destroys them.
        // sorted_splitters[i] is the Item whose key is the supremum of IPS4o
        // bucket i; the last bucket inherits the caller-supplied oldSup.
        const auto* sp = sorter.getSortedSplitters();
        for (int i = 0; i < num_buckets; ++i) {
            result.ranges[i] = Range{
                bucket_start[i],
                bucket_start[i + 1],
                i < num_buckets - 1 ? Cfg::getKey(sp[i]) : oldSup};
        }
        sorter.resetLocalData();

        result.surviving = num_buckets;
        return result;
    }

    // Merges underflowing ranges in-place on result.ranges; decrements
    // result.surviving.  No IPS4o involvement.
    // The optional maxBuckets parameter also enforces an upper bound on
    // surviving: any excess buckets are merged from the tail (the resulting
    // oversized bucket may later be recursively split).
    // Exposed as public to allow independent unit testing of the merge logic.
    void repair(SplitResult& result, std::ptrdiff_t minBucketSize,
                int maxBuckets = std::numeric_limits<int>::max()) {
        auto* r        = result.ranges;
        int& surviving = result.surviving;

        // Right-to-left pass: merge any underflowing bucket into its
        // predecessor.  Elements are contiguous in result.buf; merging is
        // just range extension plus a left-shift of the descriptor array.
        for (int i = surviving - 1; i >= 1; --i) {
            if (2 * (r[i].end - r[i].start) >= minBucketSize) continue;
            S3Q_TRACE << "event=split:repair idx=" << i << "\n";
            r[i - 1].end = r[i].end;
            r[i - 1].sup = r[i].sup;
            for (int j = i; j < surviving - 1; ++j) r[j] = r[j + 1];
            --surviving;
        }

        // If the first bucket still underflows, merge it into its successor.
        if (surviving >= 2 &&
            2 * (r[0].end - r[0].start) < minBucketSize) {
            S3Q_TRACE << "event=split:repair idx=0\n";
            r[1].start = r[0].start;
            for (int j = 0; j < surviving - 1; ++j) r[j] = r[j + 1];
            --surviving;
        }

        // Cap: IPS4o may produce more buckets than the S3Q fan-out allows.
        // Merge from the tail until surviving <= maxBuckets.  The merged
        // bucket may overflow; fixOverflowingBuckets handles that recursively.
        while (surviving > maxBuckets) {
            r[surviving - 2].end = r[surviving - 1].end;
            r[surviving - 2].sup = r[surviving - 1].sup;
            --surviving;
        }

        assert(surviving >= 1);
    }

private:
    // BufferStorage must be declared before localDataPtr_ (init order).
    typename SplitSorter::BufferStorage                       bufStorage_;
    ips4o::detail::AlignedPtr<typename SplitSorter::LocalData> localDataPtr_;
};

} // namespace s3q::detail

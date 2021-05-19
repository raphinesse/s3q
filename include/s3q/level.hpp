#pragma once

#include "bucket.hpp"
#include "classifier.hpp"
#include "sampling.hpp"
#include "util.hpp"

#include <range/v3/action/insert.hpp>
#include <range/v3/core.hpp>
#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/view/drop_exactly.hpp>
#include <range/v3/view/drop_last.hpp>
#include <range/v3/view/move.hpp>
#include <range/v3/view/transform.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

namespace s3q::detail {

template <class Cfg>
class Level {
public:
    using Bucket = ::s3q::detail::Bucket<Cfg>;
    using BucketIdx = typename Cfg::BucketIdx;
    using SplitterSampler = ::s3q::detail::SplitterSampler<>;

    // Ctor for first level
    explicit Level(SplitterSampler &sampler)
        : getSplitters(sampler), kMaxBucketSize_(Cfg::kBufBaseSize) {}

    // Ctor for any other level
    explicit Level(SplitterSampler &sampler, const Level &pred)
        : getSplitters(sampler),
          kMaxBucketSize_(pred.kMaxBucketSize_ * Cfg::kGrowthRate) {}

    bool overflow() const { return maxBufSize() > kMaxBucketSize_; }

    std::size_t size() const {
        auto size_view = rv::transform([](auto &b) { return b.buf.size(); });
        return ranges::accumulate(buckets_ | size_view, 0ul);
    }

    BucketIdx degree() const { return ssize(buckets_); }

    Bucket delMin() {
        assert(!buckets_.empty());

        // pop the front off buckets
        auto result = std::move(buckets_.front());
        buckets_.erase(buckets_.begin());

        classifier_.invalidate();

        assert(ssize(result.buf) <= kMaxBucketSize_);
        traceState("delMin:after");
        return result;
    }

    template <class Rng>
    void insert(Rng &&items) {
        assert(degree() <= Cfg::kMaxDegree);
        assert(2 * ssize(items) >= minBucketSize() / Cfg::kGrowthRate);
        assert(2 * ssize(items) >= Cfg::kBufBaseSize / Cfg::kSplitFactor);
        assert(ssize(items) <= 2 * kMaxBucketSize_);

        SizeChecker sc{*this, size() + items.size()};

        if (buckets_.size() == 0) buckets_.push_back({});

        if (buckets_.size() == 1) {
            // we have only one bucket, so just append all items onto it
            // this can only happen in the last level
            auto &b = buckets_[0].buf;
            append(b, std::forward<Rng>(items));
        } else {
            distribute(std::forward<Rng>(items));
        }

        // clear supremum on last bucket as we _might_ have invalidated it
        buckets_.back().sup = Cfg::KeyRange::sup();

        fixOverflowingBuckets(0, degree());

        traceState("insert:after");
    }

    void insertMin(Bucket &&b) {
        assert(degree() <= Cfg::kMaxDegree);
        assert(ssize(b.buf) >= kMaxBucketSize_);
        assert(ssize(b.buf) <= 3 * kMaxBucketSize_);

        SizeChecker sc{*this, size() + b.buf.size()};

        buckets_.insert(buckets_.begin(), std::move(b));

        shrinkToDegree(Cfg::kMaxDegree - Cfg::kSplitFactor + 1);
        splitAt(0);

        traceState("insertMin:after");
    }

    void flushMaxBufInto(Level &next_level) {
        is_last_ = false;
        flushMaxBufInto</*flush_all=*/false>(next_level);
    }

    void refillFrom(Level &next_level) {
        assert(degree() == Cfg::kMinDegree + 1);
        assert(next_level.degree() > 0);

        S3Q_TRACE << "event=refill_from_next lvl=" << idx() << "\n";

        // flush max-buf (alternative would be to merge it with incoming items)
        flushMaxBufInto</*flush_all=*/true>(next_level);

        // steal min-buf from next level
        buckets_.back() = next_level.delMin();
        is_last_ = (next_level.degree() == 0);

        // Since we push at least ɑ-1 times our minBucketSize, the incoming
        // bucket must have at least as many elements
        assert(maxBufSize() >= (Cfg::kSplitFactor - 1) * minBucketSize());

        // If we did not pull the last bucket from next level, the bucket size
        // is guaranteed to be at least k/2 times that of our own min-size
        const auto full_split_threshold = minBucketSize() * Cfg::kGrowthRate;
        assert(is_last_ || maxBufSize() >= full_split_threshold / 2);

        // In any case, next level's max-size constraint must be satisfied
        assert(maxBufSize() <= Cfg::kGrowthRate * kMaxBucketSize_);

        // If we pulled next level's last bucket, it might be small enough
        if (maxBufSize() <= kMaxBucketSize_) return;

        // PERF: maybe round split_degree down to next power of two
        const auto split_degree = maxBufSize() >= full_split_threshold
                                      ? Cfg::kGrowthRate
                                      : maxBufSize() / minBucketSize();

        S3Q_TRACE << "event=split_max degree=" << split_degree << "\n";
        splitAt(degree() - 1, split_degree);
    }

private:
    using Classifier = ::s3q::detail::Classifier<Cfg>;

    struct SizeChecker {
        const Level &lvl_;
        const std::size_t sz_;
        SizeChecker(const Level &l) : lvl_(l), sz_(l.size()) {}
        SizeChecker(const Level &l, std::size_t s) : lvl_(l), sz_(s) {}
        ~SizeChecker() { assert(lvl_.size() == sz_); }
    };

    // For debug purposes
    int idx() const {
        return log2_floor(kMaxBucketSize_ / Cfg::kBufBaseSize) /
               log2_floor(Cfg::kGrowthRate);
    }

    std::ptrdiff_t maxBufSize() const {
        assert(!buckets_.empty());
        return ssize(buckets_.back().buf);
    }

    std::ptrdiff_t minBucketSize() const {
        return kMaxBucketSize_ / Cfg::kSplitFactor;
    }

    // helper for getting a bucket with a signed index
    Bucket &bucket(BucketIdx i) {
        assert(i >= 0);
        assert(i < degree());
        return *(buckets_.begin() + i);
    }

    auto regularBuckets() const { return buckets_ | rv::drop_last(1); }

    auto splitters() const {
        return regularBuckets() | rv::transform(Bucket::getSup);
    }

    template <class Rng>
    void distribute(Rng &&items) {
        SizeChecker sc{*this, size() + items.size()};

        // PERF maybe rebuild eagerly?
        if (!classifier_.valid()) {
            S3Q_TRACE << "event=rebuild_classifier lvl=" << idx() << "\n";
            classifier_.build(splitters());
        }

        auto keys_view = items | rv::transform(Cfg::getKey);
        classifier_.classify(keys_view, [this](auto c, auto it) {
            bucket(c).buf.push_back(*it.base());
        });
    }

    template <bool flush_all>
    void flushMaxBufInto(Level &next_level) {
        assert(degree() > Cfg::kMinDegree);

        S3Q_TRACE << "event=flush_max lvl=" << idx() << " size=" << maxBufSize()
                  << "\n";

        auto &max_buf = buckets_.back().buf;
        if constexpr (flush_all) {
            next_level.insert(std::move(max_buf));
            max_buf.clear();
        } else {
            assert(ssize(max_buf) >= kMaxBucketSize_);
            const auto num_remaining = num_cast<std::size_t>(minBucketSize());
            next_level.insert(max_buf | rv::drop_exactly(num_remaining));
            max_buf.resize(num_remaining);
        }

        assert(maxBufSize() <= kMaxBucketSize_);
    }

    BucketIdx fixOverflowingBuckets(BucketIdx begin, BucketIdx end) {
        SizeChecker sc{*this};
        assert(end <= degree());

        // ɑ-way split any overflowing buckets in range [begin, end-1)
        // PERF: this is quadratic in the worst case
        for (BucketIdx idx = begin; idx < end - 1; ++idx) {
            if (ssize(bucket(idx).buf) <= kMaxBucketSize_) continue;

            // split the overflowing bucket
            const auto split_end = splitAt(idx);

            // move end idx & skip over new buckets
            end = std::min(degree(), end + split_end - idx - 1);
            idx = split_end - 1;
        }

        assert(end <= degree());
        const auto kMaxSplitDegree = Cfg::kMaxDegree - Cfg::kSplitFactor + 1;
        const bool max_buf_splittable = is_last_ && end <= kMaxSplitDegree;
        if ((end < degree() || max_buf_splittable) &&
            ssize(bucket(end - 1).buf) > kMaxBucketSize_) {
            // bucket(end-1) is not a max-buf so we split it too if it overflows
            end = splitAt(end - 1);
        }

        return end;
    }

    void shrinkToDegree(const BucketIdx target_degree) {
        SizeChecker sc{*this};
        if (auto diff = degree() - target_degree; diff > 0) {
            S3Q_TRACE << "event=join lvl=" << idx() << " count=" << diff
                      << "\n";

            classifier_.invalidate();
        }

        while (degree() > target_degree) {
            // remove last regular bucket and join it onto max-buf
            auto b_it = buckets_.end() - 2;
            auto &max_buf = buckets_.back().buf;
            append(max_buf, rv::move(b_it->buf));
            buckets_.erase(b_it);
        }
    }

    BucketIdx splitAt(BucketIdx idx,
                      BucketIdx split_degree = Cfg::kSplitFactor) {
        SizeChecker sc{*this};
        assert(split_degree >= Cfg::kSplitFactor);

        // degree needs to be <= this value to be able to do an ɑ-way split
        const auto kMaxSplitSize = Cfg::kMaxDegree - Cfg::kSplitFactor + 1;

        // if any bucket in the range [kMaxSplitSize-1, kMaxDegree) overflows,
        // we have to retire itself and any following buckets into the max-buf
        if (idx >= kMaxSplitSize - 1) {
            traceState("retire");
            S3Q_TRACE << "idx=" << idx << "\n";
            // flush all buckets in range [idx, degree-1)
            shrinkToDegree(idx + 1);
            return idx;
        }
        traceState("split:before");

        // first make room for the new buckets by retiring the last few
        // buckets into the max-buf, if necessary
        shrinkToDegree(kMaxSplitSize);

        traceState("split:after_shrink");

        auto buf = std::move(bucket(idx).buf);
        auto keys_view = ranges::transform_view(buf, Cfg::getKey);
        bucket(idx).buf.clear();
        assert(minBucketSize() <= ssize(buf) / split_degree);

        // determine splitters and insert them together with empty buffers
        // the old splitter becomes the supremum of the last new bucket
        auto splitters = getSplitters(keys_view, split_degree);
        auto num_new_buckets = ssize(splitters);
        assert(num_new_buckets < split_degree);
        ranges::insert(buckets_, buckets_.begin() + idx, splitters);
        classifier_.invalidate();

        S3Q_TRACE << "event=split:splitters lvl=" << this->idx()
                  << " idx=" << idx << " degree=" << ssize(splitters) + 1
                  << "\n";

        // PERF: only use local classifier if split_degree ≪ degree()
        Classifier classifier{splitters};
        const auto split_begin = buckets_.begin() + idx;
        classifier.classify(keys_view, [split_begin](auto c, auto it) {
            split_begin[c].buf.push_back(*it.base());
        });

        // From right to left, join underflowing buckets onto their predecessors
        for (auto it = split_begin + num_new_buckets; it > split_begin; --it) {
            if (2 * ssize(it->buf) >= minBucketSize()) continue;
            S3Q_TRACE << "event=split:repair lvl=" << this->idx()
                      << " idx=" << std::distance(split_begin, it) << "\n";
            auto prev = std::prev(it);
            append(prev->buf, rv::move(it->buf));
            prev->sup = it->sup;
            buckets_.erase(it);
            --num_new_buckets;
        }

        // If first bucket underflows, join it onto its successor
        if (2 * ssize(split_begin->buf) < minBucketSize()) {
            S3Q_TRACE << "event=split:repair lvl=" << this->idx() << " idx=0"
                      << "\n";
            assert(std::next(split_begin) < buckets_.end());
            auto &next = std::next(split_begin)->buf;
            append(next, rv::move(split_begin->buf));
            buckets_.erase(split_begin);
            --num_new_buckets;
        }

        assert(num_new_buckets >= 0);

        return fixOverflowingBuckets(idx, idx + num_new_buckets + 1);
    }

    void traceState(const char *event_name) {
        S3Q_TRACE << "event=Level::" << event_name << " lvl=" << idx()
                  << " max_size=" << kMaxBucketSize_ << " degree=" << degree()
                  << " bucket_sizes="
                  << rv::transform(buckets_,
                                   [](const Bucket &b) { return b.buf.size(); })
                  << "\n";
    }

    bool is_last_ = true;

    SplitterSampler &getSplitters;

    const std::ptrdiff_t kMaxBucketSize_;

    // PERF: use compact linked list for buckets (needs in-place partitioning)
    std::vector<Bucket> buckets_;

    Classifier classifier_;
};

} // namespace s3q::detail

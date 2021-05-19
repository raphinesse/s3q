#pragma once

#include "level.hpp"
#include "sampling.hpp"
#include "util.hpp"

#include <range/v3/view/transform.hpp>

#include <cassert>
#include <cstddef>
#include <deque>
#include <iterator>
#include <utility>

namespace s3q::detail {

template <class Cfg>
class BatchedPriorityQueue {
    using Level = ::s3q::detail::Level<Cfg>;
    using SplitterSampler = ::s3q::detail::SplitterSampler<>;

public:
    using Bucket = typename Level::Bucket;

    std::size_t size() const { return size_; }

    template <class Rng>
    void insert(Rng &&items) {
        size_ += items.size();

        auto first_lvl = levels_.begin();
        first_lvl->insert(std::forward<Rng>(items));

        // flush any overflowing buffers starting from first_lvl
        handleMaxBufOverflowFrom(first_lvl);

        traceState("insert:after");
    }

    void insertMin(Bucket &&b) {
        size_ += b.buf.size();

        auto first_lvl = levels_.begin();
        first_lvl->insertMin(std::move(b));

        // flush any overflowing buffers starting from first_lvl
        handleMaxBufOverflowFrom(first_lvl);

        traceState("insertMin:after");
    }

    Bucket delMin() {
        // remove & save min-buf from finest level
        auto min_bucket = levels_[0].delMin();

        // refill any levels whose degree underflows (if possible)
        handleDegreeUnderflow();

        size_ -= min_bucket.buf.size();

        traceState("delMin:after");

        return min_bucket;
    }

private:
    // Using a deque to avoid expensive copying & invalidation of iterators
    // PERF: use vector w/ stack allocation & static max-size?
    using Levels = std::deque<Level>;

    /**
     * Flushes all overflowing max-buffers starting from begin.
     * @param begin a level that just had items inserted into it
     */
    void handleMaxBufOverflowFrom(const typename Levels::iterator begin) {
        auto lvl = begin;
        const auto last_lvl = std::prev(levels_.end());

        // flush max-buffers from left to right until we reach a non-full one
        while (lvl < last_lvl && lvl->overflow()) {
            auto next_lvl = std::next(lvl);
            lvl->flushMaxBufInto(*next_lvl);
            lvl = next_lvl;
        }

        // lvl is the rightmost level into which items have been inserted
        // if we reached the last level and it overflows too, add a new level
        if (lvl == last_lvl && lvl->overflow()) {
            S3Q_TRACE << "event=add_lvl"
                      << " idx=" << levels_.size() << "\n";

            // TODO: Shouldn't we guaranteee a degree of one more?
            assert(lvl->degree() > Cfg::kMaxDegree - Cfg::kSplitFactor);

            // Add new level and flush max-buf into it
            levels_.emplace_back(sampler_, *lvl);
            lvl->flushMaxBufInto(levels_.back());
        }
    }

    /**
     * Refills any levels whose degree underflows (if possible).
     * @pre first level just had a bucket removed
     */
    void handleDegreeUnderflow() {
        auto lvl = levels_.begin();
        const auto last_lvl = std::prev(levels_.end());
        constexpr auto kRefillThreshold = Cfg::kMinDegree + 1;

        // if lvl's degree underflows, steal first bucket from next level
        // recurse if next level underflows too
        while (lvl < last_lvl && lvl->degree() <= kRefillThreshold) {
            auto next_lvl = std::next(lvl);
            lvl->refillFrom(*next_lvl);
            if (lvl->overflow()) {
                // A bad split can cause the receiving level to overflow
                lvl->flushMaxBufInto(*next_lvl);
            }
            lvl = next_lvl;
        }

        // if we did not do anything, just return
        // this also means we always preserve at least one level
        if (lvl == levels_.begin()) return;

        // if the last level has been emptied, remove it and return
        if (lvl == last_lvl && lvl->degree() == 0) {
            return levels_.pop_back();
        }

        // lvl might have been pushed to during flushMaxBufInto(lvl)
        // and its max-buffer could now be overflowing. So we flush all
        // overflowing buffers starting from lvl.
        handleMaxBufOverflowFrom(lvl);
    }

    void traceState(const char *event_name) {
        S3Q_TRACE << "event=BatchedPriorityQueue::" << event_name
                  << " size=" << size_ << " levels="
                  << rv::transform(levels_,
                                   [](const Level &l) { return l.degree(); })
                  << "\n";
    }

    // The total number of items in the queue
    std::size_t size_ = 0;

    SplitterSampler sampler_;

    // sorted from finest to coarsest (ascending order of elements)
    Levels levels_{Level(sampler_)};
};

} // namespace s3q::detail

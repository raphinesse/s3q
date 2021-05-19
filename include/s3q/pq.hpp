#pragma once

#include "batched_pq.hpp"
#include "heap.hpp"
#include "util.hpp"

#include <range/v3/algorithm/partition.hpp>
#include <range/v3/view/move.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

namespace s3q::detail {

template <class Cfg>
class PriorityQueue {
    using BatchedPriorityQueue = ::s3q::detail::BatchedPriorityQueue<Cfg>;
    using Bucket = typename BatchedPriorityQueue::Bucket;
    using Buffer = typename Bucket::Buffer;
    using Heap = ::s3q::detail::Heap<Cfg>;

public:
    using Item = typename Cfg::Item;

    PriorityQueue() {
        minBuf().reserve(Cfg::kBufBaseSize + 1);

        // add sentinel
        minBuf().resize(1);
        Cfg::getKey(minBuf()[0]) = Cfg::KeyRange::inf();
    }

    std::size_t size() const {
        return Heap::size(min_bucket_.buf) + max_buffer_.size() +
               backend_.size();
    }

    bool empty() const { return size() == 0; }

    const Item &top() const {
        assert(!empty());
        return Heap::top(min_bucket_.buf);
    }

    void push(Item item) {
        assert(Cfg::KeyRange::contains(Cfg::getKey(item)));
        if (Cfg::getKey(item) > min_bucket_.sup) {
            insertIntoMaxBuf(std::move(item));
        } else {
            insertIntoMinBuf(std::move(item));
        }
    }

    Item pop() {
        assert(!empty());
        auto item = popMinBuf();
        if (Heap::empty(minBuf()) && !empty()) refillMinBuf();
        return item;
    }

private:
    static constexpr bool overflow(const Buffer &buf) {
        return ssize(buf) >= Cfg::kBufBaseSize;
    }

    Buffer &minBuf() { return min_bucket_.buf; }

    void insertIntoMaxBuf(Item item) {
        max_buffer_.push_back(std::move(item));

        if (ssize(max_buffer_) >= Cfg::kBufBaseSize) {
            // Flush max-buffer
            backend_.insert(std::move(max_buffer_));
            max_buffer_.clear();
        }
    }

    void insertIntoMinBuf(Item item) {
        minBuf().push_back(std::move(item));

        // Flush eagerly, so we use the right splitter on next insert
        if (ssize(minBuf()) > Cfg::kBufBaseSize) {
            // remove heap sentinel
            auto last = std::prev(minBuf().end());
            minBuf()[0] = std::move(*last);
            minBuf().erase(last);

            flushMinBuf();
            Heap::make(minBuf());
        } else {
            Heap::push(minBuf());
        }
    }

    void refillMinBuf() {
        assert(Heap::empty(minBuf()));
        assert(!empty());

        if (backend_.size() == 0) {
            // remove heap sentinel
            minBuf().clear();

            // Backend is empty so max-buf is our new min-buf
            min_bucket_.sup = Cfg::KeyRange::sup();
            std::swap(minBuf(), max_buffer_);
        } else {
            // Get a new min-bucket from the backend & classify the existing
            // max-buf as either belonging to the new min-bucket or not
            min_bucket_ = backend_.delMin();
            reclassifyMaxBuf();
            if (ssize(minBuf()) > Cfg::kBufBaseSize) flushMinBuf();
        }

        Heap::make(minBuf());
    }

    void flushMinBuf() {
        // ɑ-way split min-bucket, keep the min and push rest into backend
        backend_.insertMin(std::move(min_bucket_));
        min_bucket_ = backend_.delMin();
    }

    void reclassifyMaxBuf() {
        // move all items from max-buf that are <= sup(min-buf) to min-buf
        auto is_max = [sup = min_bucket_.sup](auto k) { return sup < k; };
        auto min_begin = ranges::partition(max_buffer_, is_max, Cfg::getKey);
        auto min_items = ranges::subrange(min_begin, max_buffer_.end());
        append(minBuf(), rv::move(min_items));
        max_buffer_.erase(min_items.begin(), min_items.end());
    }

    Item popMinBuf() {
        auto &b = minBuf();
        assert(!Heap::empty(b));

        auto item = Heap::top(b);
        Heap::pop(b);
        b.pop_back();
        return item;
    }

    Bucket min_bucket_;
    Buffer max_buffer_;
    BatchedPriorityQueue backend_;
};

} // namespace s3q::detail

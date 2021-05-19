#pragma once

#include <spq/knheap.C>

#include <cassert>
#include <cstddef>
#include <limits>

/**
 * Provides information on the supremum and infimum of a given numeric type.
 */
template <typename T> struct NumberRange {
    using limits = std::numeric_limits<T>;

    static constexpr T inf() noexcept {
        return limits::has_infinity ? -limits::infinity() : limits::lowest();
    }

    static constexpr T sup() noexcept {
        return limits::has_infinity ? limits::infinity() : limits::max();
    }

    static bool contains(T k) { return inf() < k && k < sup(); };
};

template <typename T> class SequenceHeap {
protected:
    using key_type = decltype(T::key);
    using value_type = decltype(T::value);

    using key_limits = NumberRange<key_type>;

    KNHeap<key_type, value_type> heap_;

public:
    //! Allocates an empty heap.
    explicit SequenceHeap() : heap_(key_limits::sup(), key_limits::inf()) {}

    // Disable {copy,move} ctor and assignment operator
    SequenceHeap(SequenceHeap const &) = delete;
    SequenceHeap &operator=(SequenceHeap const &) = delete;

    //! Returns the number of items in the heap.
    std::size_t size() const noexcept {
        return static_cast<std::size_t>(heap_.getSize());
    }

    //! Returns true if the heap has no items, false otherwise.
    bool empty() const noexcept { return size() == 0; }

    //! Inserts a new item.
    void push(const T &item) {
        // Assert that we do not insert sentinel keys
        assert(key_limits::contains(item.key));

        heap_.insert(item.key, item.value);
    }

    //! Returns the top item.
    T top() const noexcept {
        assert(!empty());
        T min;
        heap_.getMin(&min.key, &min.value);
        return min;
    }

    //! Removes the top item.
    void pop() {
        assert(!empty());
        T min;
        heap_.deleteMin(&min.key, &min.value);
    }
};

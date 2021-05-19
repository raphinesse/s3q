#pragma once

#include "util.hpp"

#include <range/v3/core.hpp>

#include <cassert>
#include <cstdint>
#include <functional>
#include <iterator>
#include <utility>

namespace s3q::detail {

template <class Cfg>
class Heap {
    using Index = int_fast32_t;
    using Item = typename Cfg::Item;
    using KeyRange = typename Cfg::KeyRange;

    static constexpr std::greater<> comp_{};

public:
    template <class Rng>
    static const Item &top(const Rng &r) {
        assert(hasSentinel(r));
        return ranges::cbegin(r)[1];
    }

    template <class Rng>
    static auto size(const Rng &r) {
        assert(hasSentinel(r));
        return ranges::size(r) - 1;
    }

    template <class Rng>
    static bool empty(const Rng &r) {
        return size(r) == 0;
    }

    template <class Rng>
    static void make(Rng &&r) {
        assert(ranges::size(r) > 0);

        // put a sentinel at index 0
        r.push_back(*ranges::cbegin(r));
        Cfg::getKey(*ranges::begin(r)) = KeyRange::inf();

        make_heap(ranges::begin(r) + 1, ranges::end(r), keyGreater);
    }

    // like ranges::push_heap except that we require a sentinel at idx 0
    template <class Rng>
    static void push(Rng &&r) {
        assert(hasSentinel(r));
        bubbleUpLastFrom(r, ssize(r) - 1);
    }

    // like ranges::pop_heap except that we require a sentinel at idx 0 and the
    // last element of r is in an undefined state after the pop
    template <class Rng>
    static void pop(Rng &&r) {
        assert(hasSentinel(r));
        const Index maxIdx = ssize(r) - 1;
        assert(maxIdx > 0);
        const auto data = ranges::begin(r);

        // first move up elements on a min-path
        Index hole = 1;
        for (Index succ = 2; succ < maxIdx; succ <<= 1) {
            succ += keyLess(data[succ + 1], data[succ]);
            data[hole] = data[succ];
            hole = succ;
        }

        // then bubble up rightmost element
        bubbleUpLastFrom(r, hole);
    }

private:
    template <class Rng>
    static bool hasSentinel(const Rng &r) {
        assert(ssize(r) > 0);
        return Cfg::getKey(*ranges::cbegin(r)) == KeyRange::inf();
    }

    static bool keyLess(const Item &a, const Item &b) {
        return Cfg::getKey(a) < Cfg::getKey(b);
    }

    static bool keyGreater(const Item &a, const Item &b) {
        return Cfg::getKey(a) > Cfg::getKey(b);
    }

    template <class Rng>
    static void bubbleUpLastFrom(Rng &&r, Index hole) {
        const auto data = ranges::begin(r);
        const auto el = *ranges::crbegin(r);

        // bubble up hole (must terminate since sentinel at 0)
        for (Index pred = hole >> 1; keyLess(el, data[pred]); pred >>= 1) {
            data[hole] = data[pred];
            hole = pred;
        }

        // finally move element to hole
        data[hole] = el;
    }

    // The following code is an adapted version of opt5.h++ from
    // "Heap construction—50 years later" by Edelkamp, Elmasry and Katajainen
    // see http://hjemmesider.diku.dk/~jyrki/Myris/EEK2017aJ.html

    // It is not particularly important for the overall PQ performance
    // but it brings down the mispredictions compared to std::make_heap

    // TODO further simplify this by using our own sift_up code
    // and by making use of the sentinel at index 0
    static constexpr Index root() { return 0; }
    static Index parent(Index i) { return (i - 1) / 2; }
    static Index left_child(Index i) { return (i << 1) + 1; }

    template <typename iterator, typename comparator>
    static void sift_up(iterator a, Index j, comparator less) {
        using element = typename std::iterator_traits<iterator>::value_type;
        element in = std::move(a[j]);
        while (j > root()) {
            Index i = parent(j);
            if (!less(a[i], in)) break;
            a[j] = std::move(a[i]);
            j = i;
        }
        a[j] = std::move(in);
    }

    template <typename iterator, typename comparator>
    static void make_heap(iterator first, iterator past, comparator less) {
        using element = typename std::iterator_traits<iterator>::value_type;

        Index const n = past - first;
        if (n < 2) return;
        if (n == 2 && less(first[0], first[1])) {
            return std::swap(first[0], first[1]);
        }

        Index const m = (n bitand 1) ? n : n - 1;
        Index i = parent(m - 1);
        Index j = i;
        Index hole = j;
        element in = std::move(first[j]);
        while (true) {
            if (i == j) {
                hole = j;
                in = std::move(first[j]);
            }

            j = left_child(j);
            j += less(first[j], first[j + 1]);
            first[hole] = std::move(first[j]);
            hole = less(in, first[j]) ? j : hole;

            if (left_child(j) >= m) {
                first[hole] = std::move(in);
                if (i == root()) break;
                i = i - 1;
                j = i;
            }
        }
        sift_up(first, n - 1, less);
    }
};

} // namespace s3q::detail

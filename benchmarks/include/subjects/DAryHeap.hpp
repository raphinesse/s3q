#pragma once

#include <tlx/container/d_ary_heap.hpp>

template <unsigned Arity> struct DAryHeap {
    template <typename T> using type = tlx::DAryHeap<T, Arity>;
};

#pragma once

#include <functional>
#include <queue>
#include <vector>

template <typename T>
using StdMinQueue = std::priority_queue<T, std::vector<T>, std::greater<T>>;

template <typename T> struct StdQueue : StdMinQueue<T> {
    using key_type = typename StdMinQueue<T>::value_type;
};

#pragma once

#include <s3q/s3q.hpp>

#include <cassert>
#include <cstddef>
#include <vector>

template <typename T>
class S3QBH {
    struct BaseCfg : s3q::DefaultCfg {
        using Item = T;
    };
    using Cfg = s3q::detail::ExtendedCfg<BaseCfg>;
    using Heap = s3q::detail::Heap<Cfg>;

    std::vector<T> data;

public:
    //! Allocates an empty heap.
    explicit S3QBH() {
        // add sentinel
        data.resize(1);
        Cfg::getKey(data[0]) = Cfg::KeyRange::inf();
    }

    // Disable {copy,move} ctor and assignment operator
    S3QBH(S3QBH const &) = delete;
    S3QBH &operator=(S3QBH const &) = delete;

    std::size_t size() const noexcept { return Heap::size(data); }
    bool empty() const noexcept { return size() == 0; }
    T top() const noexcept { return Heap::top(data); }

    //! Inserts a new item.
    void push(const T &item) {
        // Assert that we do not insert sentinel keys
        assert(Cfg::KeyRange::contains(Cfg::getKey(item)));

        data.push_back(item);
        Heap::push(data);
    }

    //! Removes the top item.
    void pop() {
        Heap::pop(data);
        data.pop_back();
    }
};

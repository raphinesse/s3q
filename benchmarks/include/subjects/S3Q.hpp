#pragma once

#include <s3q/s3q.hpp>
#include <cstddef>

template <int logK, int logM>
class S3Q {
    template <typename T>
    struct Cfg : s3q::DefaultCfg {
        using Item = T;
        static constexpr std::ptrdiff_t kBufBaseSize =
            (1l << logM) / sizeof(Item);
        static constexpr int kLogMaxDegree = logK;
    };

public:
    template <typename T>
    class type : public s3q::PriorityQueue<Cfg<T>> {};
};

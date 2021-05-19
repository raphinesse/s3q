#pragma once

#include "util.hpp"

#include <XoshiroCpp.hpp>

#include <range/v3/action/sort.hpp>
#include <range/v3/core.hpp>
#include <range/v3/view/drop_exactly.hpp>
#include <range/v3/view/stride.hpp>
#include <range/v3/view/unique.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace s3q::detail {

namespace lemire {

// Given uint type T, returns an uint type at least double as wide as T
// clang-format off
template <class> struct wider_uint;
template <> struct wider_uint<uint32_t> { using type = uint64_t; };
#ifdef __SIZEOF_INT128__
__extension__ using uint128_t = unsigned __int128;
template <> struct wider_uint<uint64_t> { using type = uint128_t; };
#endif
template <class T> using wider_uint_t = typename wider_uint<T>::type;
// clang-format on

/**
 * Lemire's nearly divisionless algorithm.
 *
 * @see https://arxiv.org/abs/1805.10941
 * Fast Random Integer Generation in an Interval
 * ACM Transactions on Modeling and Computer Simulation 29 (1), 2019
 *
 * @param g UniformRandomBitGenerator with range [0, 2^L)
 * @param range ∊ [0, 2^L)
 * @return an unbiased random number in [0,range)
 */
template <class Urbg, class U>
U uniformRandomInt(Urbg &g, U range) {
    // Wide type that can hold the full result of u1 * u2
    using W = wider_uint_t<U>;
    using U_traits = std::numeric_limits<U>;

    // Assert that random bit generator uses full bit range
    static_assert(Urbg::min() == 0u);
    static_assert(Urbg::max() == U_traits::max());

    W product = W(g()) * W(range);
    U low = U(product);               // product % 2^L
    if (low < range) {                // Apply rejection method
        U threshold = -range % range; // (2^L - range) % range
        while (low < threshold) {
            product = W(g()) * W(range);
            low = U(product);
        }
    }
    return U(product >> U_traits::digits);
}

} // namespace lemire

template <class Urbg = XoshiroCpp::Xoshiro128StarStar>
class SplitterSampler {
    using UrbgResult = typename Urbg::result_type;

    Urbg urbg_;

    static constexpr int oversamplingFactor(std::size_t n) {
        return std::max(1, log2_floor(n));
    }

    template <class Rng>
    auto selectSample(Rng &&keys, std::ptrdiff_t num_samples) {
        std::vector<ranges::range_value_t<Rng>> sample;
        sample.reserve(num_cast<std::size_t>(num_samples));

        auto n = num_cast<uint32_t>(keys.size());
        while (num_samples--) {
            const auto i = lemire::uniformRandomInt(urbg_, n--);
            sample.emplace_back(keys.begin()[i]);
        }
        return sample;
    }

public:
    SplitterSampler() {}
    explicit SplitterSampler(UrbgResult seed) : urbg_(seed) {}

    template <class Rng>
    auto operator()(Rng &&keys, std::ptrdiff_t num_buckets) {
        const auto step = oversamplingFactor(keys.size());
        const auto sample_size = step * num_buckets - 1;
        assert(sample_size <= ssize(keys));

        using namespace ranges;

        auto sample = selectSample(keys, sample_size) | actions::sort;

        auto splitters = sample | views::drop_exactly(step - 1) |
                         views::stride(step) | views::unique;

        // PERF: write splitters view back into sample vector
        return splitters | to<std::vector>();
    }
};

} // namespace s3q::detail

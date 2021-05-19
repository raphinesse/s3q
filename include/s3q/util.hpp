#pragma once

#include <range/v3/action/insert.hpp>
#include <range/v3/core.hpp>

#include <cassert>
#include <cstddef>
#include <iostream>
#include <limits>
#include <type_traits>
#include <utility>

// replace true with false to show verbose trace output
#define S3Q_TRACE if (true) {} else std::cerr << "TRACE "

namespace s3q::detail {

// Convenience alias for ranges::views
namespace rv = ranges::views;

/**
 * A poor man's boost::numeric_cast that uses assert to check for loss of range.
 *
 * Inspired by: https://codereview.stackexchange.com/q/5515/11013
 */
template <typename TargetT, typename SourceT>
constexpr TargetT num_cast(SourceT input) {
    static_assert(std::is_arithmetic<SourceT>::value);
    static_assert(std::is_arithmetic<TargetT>::value);

    auto output = static_cast<TargetT>(input);

    // We can cast back to SourceT without losing information
    assert(static_cast<SourceT>(output) == input);

    // Output is positive iff input is positive
    assert((SourceT(0) < input) == (TargetT(0) < output));

    return output;
}

template <class C>
constexpr auto ssize(const C &c) {
    using R = std::common_type_t<std::ptrdiff_t,
                                 std::make_signed_t<decltype(c.size())>>;
    return num_cast<R>(c.size());
}

/** Calculates ⌊log2 n⌋ */
inline constexpr int log2_floor(unsigned long n) {
    return std::numeric_limits<unsigned long>::digits - 1 - __builtin_clzl(n);
}
inline constexpr int log2_floor(long n) {
    return log2_floor(num_cast<unsigned long>(n));
}

/** Calculates ⌈log2 n⌉ for n > 1 */
inline constexpr int log2_ceil(unsigned long n) {
    assert(n > 1);
    return 1 + log2_floor(n - 1);
}
inline constexpr int log2_ceil(long n) {
    return log2_ceil(num_cast<unsigned long>(n));
}

/**
 * Provides information on the supremum and infimum of a given numeric type.
 */
template <typename T>
struct NumberRange {
    using limits = std::numeric_limits<T>;

    static constexpr T inf() noexcept {
        return limits::has_infinity ? -limits::infinity() : limits::lowest();
    }

    static constexpr T sup() noexcept {
        return limits::has_infinity ? limits::infinity() : limits::max();
    }

    static bool contains(T k) { return inf() < k && k < sup(); };
};

template <class Rng1, class Rng2>
void append(Rng1 &&r1, Rng2 &&r2) {
    ranges::insert(std::forward<Rng1>(r1), r1.end(), std::forward<Rng2>(r2));
}

} // namespace s3q::detail

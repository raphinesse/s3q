#pragma once

#include "util.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace s3q {

struct DefaultCfg {
    // should be a signed integer to avoid unsigned arithmetic pitfalls
    using BucketIdx = std::ptrdiff_t;

    // Keep the size of this as small as possible
    // Needs to be default-constructible
    struct Item {
        int key, value;
    };

    // Let M = kL1CacheSize, B = kL1CacheLineSize (both in Bytes)

    // Should be something like M / (4*sizeof(Item))
    static constexpr std::ptrdiff_t kBufBaseSize = (1l << 15) / sizeof(Item);

    // Should be something like M / B
    static constexpr int kLogMaxDegree = 6;
};

namespace detail {

// Default GetKey functor for arithmetic Item or Item::key
template <class Cfg, class Enable = void>
struct GetKey {
    template <class Item>
    constexpr decltype(auto) operator()(Item &&item) const noexcept {
        if constexpr (std::is_arithmetic_v<std::remove_reference_t<Item>>) {
            return std::forward<Item>(item);
        } else {
            // W/out the parens here, decltype(auto) resolves to Key
            return (std::forward<Item>(item).key);
        }
    }
};

// Use user-provided GetKey functor
template <class Cfg>
struct GetKey<Cfg, std::void_t<decltype(Cfg::GetKey)>> : Cfg::GetKey {};

/**
 * Extends user-config Base with derived values.
 *
 * Base could be derived from DefaultCfg to provide defaults.
 */
template <class Base = DefaultCfg>
struct ExtendedCfg : Base {
    using typename Base::BucketIdx;
    using typename Base::Item;

    using GetKey = ::s3q::detail::GetKey<Base>;
    using Key = std::remove_reference_t<decltype(GetKey()(Item()))>;
    using KeyRange = detail::NumberRange<Key>;

    static constexpr GetKey getKey{};

    using Base::kLogMaxDegree;
    static constexpr BucketIdx kMaxDegree = 1l << kLogMaxDegree;
    static constexpr BucketIdx kMinDegree = kMaxDegree >> 1;
    static constexpr BucketIdx kSplitFactor = 1l << (kLogMaxDegree >> 1);
    static constexpr BucketIdx kGrowthRate = kMaxDegree - kMinDegree;

    // During an insert, we can receive up to 3 times our max bucket size. If
    // all those items end up in a single bucket, we want a regular split to
    // produce buckets of legal size.
    static_assert(kSplitFactor >= 4);
};

} // namespace detail

} // namespace s3q

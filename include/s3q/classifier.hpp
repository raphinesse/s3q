#pragma once

#include "util.hpp"

#include <ips4o/classifier.hpp>

#include <range/v3/algorithm/copy.hpp>
#include <range/v3/algorithm/is_sorted.hpp>
#include <range/v3/core.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/repeat.hpp>
#include <range/v3/view/take_exactly.hpp>

#include <cassert>
#include <functional>
#include <utility>

namespace s3q::detail {

template <class Cfg>
class Classifier {
public:
    Classifier() {}

    template <class Rng>
    explicit Classifier(const Rng &rng) {
        build(rng);
    }

    bool valid() const { return num_buckets_ >= 2; };
    void invalidate() { num_buckets_ = 0; };

    // All of the following methods require:
    // sized_range<Rng>
    // && random_access_range<Rng>
    // && convertible_to<range_value_t<Rng>, Cfg::Key>

    template <class Rng>
    void build(const Rng &sorted_keys) {
        assert(!ranges::empty(sorted_keys));
        assert(Cfg::KeyRange::contains(*ranges::cbegin(sorted_keys)));
        assert(Cfg::KeyRange::contains(*ranges::crbegin(sorted_keys)));
        assert(ranges::is_sorted(sorted_keys));

        const auto num_splitters = ssize(sorted_keys);
        num_buckets_ = num_splitters + 1;

        const auto log_buckets = log2_ceil(num_buckets_);
        const auto next_power_of_2 = 1l << log_buckets;

        // pad keys with supremum to next power of two
        constexpr auto key_sup = Cfg::KeyRange::sup();
        auto padded_keys = rv::concat(sorted_keys, rv::repeat(key_sup)) |
                           rv::take_exactly(next_power_of_2 - 1);

        // PERF: avoid the copying here?
        ranges::copy(padded_keys, classifier_.getSortedSplitters());

        classifier_.build(log_buckets);

        // Check that we properly classify elements for the last bucket
        assert(classifier_.template classify<false>(
                   *ranges::crbegin(sorted_keys) + 1) == num_splitters);
    }

    template <class Rng, class Yield>
    void classify(const Rng &subjects, Yield &&yield) const {
        assert(valid());

        classifier_.template classify<false>(ranges::cbegin(subjects),
                                             ranges::cend(subjects),
                                             std::forward<Yield>(yield));
    }

private:
    struct Ips4oCfg {
        using value_type = typename Cfg::Key;
        using bucket_type = typename Cfg::BucketIdx;
        using less = std::less<value_type>;

        // ips4o's Classifier only has space for (kMaxBuckets / 2) splitters
        static constexpr int kMaxBuckets = Cfg::kMaxDegree * 2;
        static constexpr int kUnrollClassifier = 7;

        // Unroll at most to the minimum amount of items to be inserted
        static_assert(kUnrollClassifier <=
                      Cfg::kBufBaseSize / Cfg::kSplitFactor / 2);
    };

    typename Cfg::BucketIdx num_buckets_ = 0;

    ips4o::detail::Classifier<Ips4oCfg> classifier_{typename Ips4oCfg::less()};
};

} // namespace s3q::detail

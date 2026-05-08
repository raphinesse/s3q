#pragma once

#include "classifier.hpp"
#include "sampling.hpp"

namespace s3q::detail {

/**
 * Encapsulates split-time splitter sampling and item classification.
 *
 * During a split operation this class:
 *   1. Samples splitter keys from the bucket being split via the shared
 *      SplitterSampler, building an internal per-split Classifier from
 *      the result.
 *   2. Classifies the bucket's items into the newly-created sub-buckets
 *      using that Classifier.
 *
 * This mirrors the Classifier class but is dedicated to split operations,
 * keeping split-specific state out of Level.
 */
template <class Cfg>
class Splitter {
public:
    using SplitterSampler = ::s3q::detail::SplitterSampler<>;

    explicit Splitter(SplitterSampler &sampler) : sampler_(sampler) {}

    /**
     * Samples splitters from keys and prepares the internal Classifier.
     *
     * @param keys        a random-access range of keys from the bucket to split
     * @param num_buckets desired number of result buckets (>= Cfg::kSplitFactor)
     * @return vector of sampled splitter key values
     */
    template <class Rng>
    auto operator()(const Rng &keys, std::ptrdiff_t num_buckets) {
        auto splitters = sampler_(keys, num_buckets);
        classifier_.build(splitters);
        return splitters;
    }

    /**
     * Classifies items into the sub-buckets determined by the last call to
     * operator().
     */
    template <class Rng, class Yield>
    void classify(const Rng &items, Yield &&yield) const {
        classifier_.classify(items, std::forward<Yield>(yield));
    }

private:
    SplitterSampler &sampler_;
    ::s3q::detail::Classifier<Cfg> classifier_;
};

} // namespace s3q::detail

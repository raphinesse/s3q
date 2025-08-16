#include <s3q/classifier.hpp>
#include <s3q/config.hpp>

#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/core.hpp>
#include <range/v3/view/iota.hpp>

#include <tlx/die.hpp>

#include <array>

struct TestCfg : s3q::detail::ExtendedCfg<> {
    static constexpr unsigned kLogMaxDegree = 2u;
};

int main() {
    using ranges::views::ints;
    s3q::detail::Classifier<TestCfg> classifier;

    { // #buckets = max
        int counts[4] = {0};
        classifier.build(std::array{2, 4, 6});
        classifier.classify(ints(1, 9), [&counts](auto cls, auto it) {
            die_unless(*it <= (int(cls) + 1) * 2);
            ++counts[cls];
        });
        die_unless(ranges::all_of(counts, [](auto c) { return c == 2; }));
    }

    { // #buckets = power of two that is less than the max
        int counts[2] = {0, 0};
        classifier.build(std::array{5});
        classifier.classify(ints(1, 11), [&counts](auto cls, auto it) {
            die_unless(*it <= (int(cls) + 1) * 5);
            ++counts[cls];
        });
        die_unless(ranges::all_of(counts, [](auto c) { return c == 5; }));
    }

    { // #buckets = not a power of two
        int counts[3] = {0};
        classifier.build(std::array{3, 6});
        classifier.classify(ints(1, 10), [&counts](auto cls, auto it) {
            die_unless(*it <= (int(cls) + 1) * 3);
            ++counts[cls];
        });
        die_unless(ranges::all_of(counts, [](auto c) { return c == 3; }));
    }
}

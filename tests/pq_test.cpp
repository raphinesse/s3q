#include <s3q/s3q.hpp>

#include <range/v3/algorithm/equal.hpp>
#include <range/v3/core.hpp>
#include <range/v3/view/generate_n.hpp>
#include <range/v3/view/indices.hpp>
#include <range/v3/view/transform.hpp>

#include <tlx/die.hpp>

#include <cstddef>
#include <vector>

struct TestCfg : s3q::DefaultCfg {
    static constexpr std::ptrdiff_t kBufBaseSize = 64;
    static constexpr int kLogMaxDegree = 4;
};

constexpr auto N = 1 << 10;
constexpr s3q::detail::GetKey<TestCfg> getKey;
constexpr auto makeItem(int i) { return TestCfg::Item{i, i}; }

int main() {
    s3q::PriorityQueue<TestCfg> pq;

    namespace views = ranges::views;
    auto keys = views::closed_indices(1, N);
    auto items = keys | views::transform(makeItem);

    for (auto i : items) {
        pq.push(i);
    }

    auto popped_items = views::generate_n([&pq]() { return pq.pop(); }, N);
    auto popped_keys =
        popped_items | views::transform(getKey) | ranges::to<std::vector>;

    die_unless(pq.empty());
    die_unless(ranges::equal(keys, popped_keys));
}

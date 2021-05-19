#include <s3q/s3q.hpp>

#include <range/v3/algorithm/minmax.hpp>
#include <range/v3/core.hpp>
#include <range/v3/view/chunk.hpp>
#include <range/v3/view/indices.hpp>
#include <range/v3/view/transform.hpp>

#include <cassert>
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
    s3q::BatchedPriorityQueue<TestCfg> bpq;

    namespace views = ranges::views;
    auto keys = views::closed_indices(1, N);
    auto items = keys | views::transform(makeItem);
    auto batches = items | views::chunk(TestCfg::kBufBaseSize);

    for (auto b : batches) {
        assert(ranges::size(b) == TestCfg::kBufBaseSize);
        bpq.insert(b);
    }

    int max_popped_key = 0;
    while (bpq.size() > 0) {
        auto bucket = bpq.delMin();
        assert(bucket.buf.size() > 0);

        auto keys = bucket.buf | views::transform(getKey);
        auto [kmin, kmax] = ranges::minmax(keys);

        assert(max_popped_key < kmin);
        assert(kmax <= bucket.sup);
        max_popped_key = kmax;
    }
}

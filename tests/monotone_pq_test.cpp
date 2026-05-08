#include <s3q/s3q.hpp>

#include <tlx/die.hpp>

#include <cstdint>
#include <random>

// Reproduces the stack overflow with monotone i32 items reported in the issue.
// Small config so the test runs fast while still triggering the degenerate
// split scenario (all items in a bucket share the same key after the initial
// random push phase with a narrow key range).
struct TestCfg : s3q::DefaultCfg {
    using Item = int32_t;
    static constexpr std::ptrdiff_t kBufBaseSize = 64;
    static constexpr int kLogMaxDegree = 4;
};

int main() {
    constexpr int N = 1 << 14;

    s3q::PriorityQueue<TestCfg> pq;

    std::mt19937 rng(42);

    // Push N random values with a narrow key range to create many duplicates
    // (birthday-paradox effect ensures many buckets fill with equal-key items).
    std::uniform_int_distribution<int32_t> init_dist(0, N / 4);
    for (int i = 0; i < N; ++i) {
        pq.push(init_dist(rng));
    }

    // N pop-push pairs where pushed values are monotonically increasing.
    // This is the usage pattern described in the issue.
    std::uniform_int_distribution<int32_t> delta_dist(1, 1000);
    for (int i = 0; i < N; ++i) {
        int32_t v = pq.pop();
        pq.push(v + delta_dist(rng));
    }

    // Drain the queue and verify monotone order
    int32_t prev = std::numeric_limits<int32_t>::min();
    while (!pq.empty()) {
        int32_t v = pq.pop();
        die_unless(v >= prev);
        prev = v;
    }
}

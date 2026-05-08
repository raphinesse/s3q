#include <s3q/splitter.hpp>
#include <s3q/s3q.hpp>

#include <range/v3/algorithm/minmax.hpp>
#include <range/v3/core.hpp>
#include <range/v3/view/chunk.hpp>
#include <range/v3/view/indices.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/range/conversion.hpp>

#include <tlx/die.hpp>

#include <algorithm>
#include <cstddef>
#include <vector>

// ─── Test configuration ──────────────────────────────────────────────────────

struct TestBase : s3q::DefaultCfg {
    static constexpr std::ptrdiff_t kBufBaseSize = 64;
    static constexpr int kLogMaxDegree = 4;
};

// Extended config for direct use with BucketSplitter<>
using TestCfg = s3q::detail::ExtendedCfg<TestBase>;
using Splitter = s3q::detail::BucketSplitter<TestCfg>;
using Buffer   = Splitter::Buffer;
using Result   = Splitter::SplitResult;
using Range    = Result::Range;
using diff_t   = Splitter::diff_t;
using Key      = TestCfg::Key;

constexpr auto makeItem(int i) noexcept { return TestBase::Item{i, i}; }

// Make a Buffer with 'count' items whose keys start at 'start'.
static Buffer makeBuffer(int start, int count) {
    Buffer buf;
    buf.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) buf.push_back(makeItem(start + i));
    return buf;
}

// Sum of all range sizes in a SplitResult.
static int totalItems(const Result& r) {
    int total = 0;
    for (int i = 0; i < r.surviving; ++i)
        total += static_cast<int>(r.ranges[i].end - r.ranges[i].start);
    return total;
}

// Build a SplitResult by hand (without running IPS4o) for repair() tests.
// The 'buf' field is left empty because repair() does not access it.
static Result makeManualResult(std::initializer_list<Range> ranges) {
    Result r;
    r.surviving = 0;
    for (auto& rng : ranges) r.ranges[r.surviving++] = rng;
    return r;
}

// ─── Group A — BucketSplitter::partition (IPS4o boundary correctness) ────────

static void testPartitionAllItemsPresent() {
    // A.1 — No item is lost or duplicated.
    constexpr int N = 128;
    Splitter sp;
    auto result = sp.partition(makeBuffer(1, N),
                               TestCfg::KeyRange::sup());
    die_unless(result.surviving >= 1);
    die_unless(totalItems(result) == N);
}

static void testPartitionBoundaryOrdering() {
    // A.2 — Every item in range[i] satisfies key <= ranges[i].sup and
    //       key > ranges[i-1].sup (for i > 0).
    constexpr int N = 128;
    Splitter sp;
    auto result = sp.partition(makeBuffer(1, N),
                               TestCfg::KeyRange::sup());

    for (int i = 0; i < result.surviving; ++i) {
        auto lo = result.ranges[i].start;
        auto hi = result.ranges[i].end;
        auto sup_i = result.ranges[i].sup;
        Key prev_sup = i > 0 ? result.ranges[i - 1].sup : TestCfg::KeyRange::inf();
        for (diff_t j = lo; j < hi; ++j) {
            auto k = TestCfg::getKey(result.buf[static_cast<std::size_t>(j)]);
            die_unless(k <= sup_i);
            die_unless(k > prev_sup);
        }
    }
}

static void testPartitionBalancedInputNoRepairNeeded() {
    // A.3 — With a uniform distribution and minBucketSize=1, split() produces
    //       the same surviving count as partition() alone (repair changes nothing).
    constexpr int N = 128;
    Splitter sp;
    auto raw = sp.partition(makeBuffer(1, N), TestCfg::KeyRange::sup());
    int raw_surviving = raw.surviving;

    // split() with minBucketSize=1 and maxBuckets=raw_surviving — neither
    // underflow repair nor cap should fire.
    auto split_result = sp.split(makeBuffer(1, N), TestCfg::KeyRange::sup(),
                                 1, raw_surviving);
    die_unless(split_result.surviving == raw_surviving);
}

// ─── Group B — BucketSplitter::repair (merge logic, no IPS4o) ───────────────

static void testRepairRightToLeftMerge() {
    // B.4 — Last range below threshold merges into predecessor.
    // Ranges: [0,10,5], [10,20,10], [20,24,15]; minBucketSize=10
    // Range 2: size=4, 2*4=8 < 10 → merge into range 1.
    Splitter sp;
    auto r = makeManualResult({Range{0, 10, 5}, Range{10, 20, 10},
                               Range{20, 24, 15}});
    sp.repair(r, 10);
    die_unless(r.surviving == 2);
    die_unless(r.ranges[0].start == 0 && r.ranges[0].end == 10);
    die_unless(r.ranges[1].start == 10 && r.ranges[1].end == 24);
    die_unless(r.ranges[1].sup == 15);
    // No items lost: total items preserved.
    die_unless(r.ranges[0].end - r.ranges[0].start +
               r.ranges[1].end - r.ranges[1].start == 24);
}

static void testRepairMultipleConsecutiveUnderflows() {
    // B.5 — Several tail ranges all below threshold, all merged.
    // Ranges: [0,10], [10,14], [14,18], [18,20]; minBucketSize=10
    Splitter sp;
    auto r = makeManualResult({Range{0, 10, 5},  Range{10, 14, 10},
                               Range{14, 18, 15}, Range{18, 20, 20}});
    sp.repair(r, 10);
    // right-to-left: i=3 (size=2 < 5) merges into i=2 → {14,20}; surviving=3
    //               i=2 (size=6 >= 5) ok
    //               i=1 (size=4 < 5) merges into i=0 → {0,14}; surviving=2
    die_unless(r.surviving == 2);
    die_unless(r.ranges[0].start == 0 && r.ranges[0].end == 14);
    die_unless(r.ranges[1].start == 14 && r.ranges[1].end == 20);
    die_unless(r.ranges[0].end - r.ranges[0].start +
               r.ranges[1].end - r.ranges[1].start == 20);
}

static void testRepairFirstBucketUnderflow() {
    // B.6 — First range below threshold merges into successor.
    // Ranges: [0,3,5], [3,13,10], [13,23,20]; minBucketSize=10
    Splitter sp;
    auto r = makeManualResult({Range{0, 3, 5}, Range{3, 13, 10},
                               Range{13, 23, 20}});
    sp.repair(r, 10);
    // right-to-left: i=2 (size=10, ok), i=1 (size=10, ok)
    // first-bucket: size=3, 2*3=6 < 10 → merge into successor
    die_unless(r.surviving == 2);
    die_unless(r.ranges[0].start == 0 && r.ranges[0].end == 13);
    die_unless(r.ranges[1].start == 13 && r.ranges[1].end == 23);
    die_unless(r.ranges[0].end - r.ranges[0].start +
               r.ranges[1].end - r.ranges[1].start == 23);
}

static void testRepairNoUnderflow() {
    // B.7 — All ranges above threshold: surviving unchanged.
    Splitter sp;
    auto r = makeManualResult({Range{0, 10, 5}, Range{10, 20, 15},
                               Range{20, 30, 25}});
    sp.repair(r, 10);
    die_unless(r.surviving == 3);
    die_unless(r.ranges[0].start == 0  && r.ranges[0].end == 10);
    die_unless(r.ranges[1].start == 10 && r.ranges[1].end == 20);
    die_unless(r.ranges[2].start == 20 && r.ranges[2].end == 30);
}

// ─── Group C — BucketSplitter::split and Level integration ───────────────────

static void testSplitRoundTrip() {
    // C.8 — Round-trip split(): items present, boundary ordering, surviving>=1.
    constexpr int N = 128;
    constexpr auto minBucketSize = std::ptrdiff_t{1};
    constexpr int maxBuckets = TestCfg::kMaxDegree; // generous upper bound

    Splitter sp;
    auto result = sp.split(makeBuffer(1, N), TestCfg::KeyRange::sup(),
                           minBucketSize, maxBuckets);

    // All items present.
    die_unless(result.surviving >= 1);
    die_unless(totalItems(result) == N);

    // Boundary ordering: every item in range[i] satisfies
    // key <= ranges[i].sup and key > ranges[i-1].sup.
    for (int i = 0; i < result.surviving; ++i) {
        auto lo    = result.ranges[i].start;
        auto hi    = result.ranges[i].end;
        auto sup_i = result.ranges[i].sup;
        Key prev_sup = i > 0 ? result.ranges[i - 1].sup : TestCfg::KeyRange::inf();
        for (diff_t j = lo; j < hi; ++j) {
            auto k = TestCfg::getKey(
                result.buf[static_cast<std::size_t>(j)]);
            die_unless(k <= sup_i);
            die_unless(k > prev_sup);
        }
    }
}

static void testLevelContextOverflow() {
    // C.9 — Integration: insert enough items to force multiple splits through
    //       BatchedPriorityQueue; drain and verify fully sorted bucket output.
    namespace views = ranges::views;

    constexpr int N = 1 << 11; // 2048 items — well beyond kBufBaseSize
    s3q::BatchedPriorityQueue<TestBase> bpq;

    auto items  = views::closed_indices(1, N)
                | views::transform(makeItem);
    auto batches = items
                 | views::chunk(TestBase::kBufBaseSize)
                 | views::transform(ranges::to_vector);

    for (auto b : batches) bpq.insert(b);

    int max_key = 0;
    while (bpq.size() > 0) {
        auto bucket = bpq.delMin();
        die_unless(!bucket.buf.empty());

        auto keys = bucket.buf
                  | views::transform(
                        [](const TestBase::Item& it) { return it.key; });
        auto [kmin, kmax] = ranges::minmax(keys);

        // Every key in this bucket is strictly greater than all previously
        // popped keys (monotone output).
        die_unless(max_key < kmin);
        // All keys are within the bucket's supremum.
        die_unless(kmax <= bucket.sup);
        max_key = kmax;
    }

    // All N items were returned.
    die_unless(max_key == N);
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main() {
    // Group A — partition()
    testPartitionAllItemsPresent();
    testPartitionBoundaryOrdering();
    testPartitionBalancedInputNoRepairNeeded();

    // Group B — repair()
    testRepairRightToLeftMerge();
    testRepairMultipleConsecutiveUnderflows();
    testRepairFirstBucketUnderflow();
    testRepairNoUnderflow();

    // Group C — split() and integration
    testSplitRoundTrip();
    testLevelContextOverflow();
}

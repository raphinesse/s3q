# Design: Replace Scatter-Based Bucket Splitting with IPS4o In-Place Partitioning

## Problem

`s3q::detail::Level<>::splitAt()` accounts for ~40% of all L1 cache misses according
to Callgrind. The root cause is the scatter pattern: after sampling splitters and
building a classifier, every item is individually dispatched (`push_back`) into one of
α destination `std::vector` buffers. Each write touches a different, likely cold vector,
producing a storm of cache misses proportional to the number of items split.

## Proposed Solution

Replace the scatter with IPS4o's block-permutation in-place partitioning. IPS4o
classifies and rearranges items within the bucket's own contiguous memory using
cache-line-aligned write buffers, then flushes them back into the array. After one
level of partitioning the array is fully rearranged and each bucket occupies a
contiguous subrange. The subranges are then copied into the destination `Bucket::buf`
vectors.

## Algorithm Sketch

```
Current splitAt():
  1. Sample splitters
  2. Build local Classifier over splitters
  3. For each item in buf: push_back into target vector  ← cache miss storm
  4. Repair: merge underflowing vectors onto neighbors

Proposed splitAt():
  1. IPS4o partitionOnce(buf.begin(), buf.end(), bucket_start[])
       → buf is rearranged in place; bucket_start[i..i+1] = subrange of bucket i
       → IPS4o's internal sampler chose the splitters
  2. Repair: adjust bucket_start[] indices for underflowing subranges (pure arithmetic,
     no allocation)
  3. Read back splitters from IPS4o classifier to assign bucket suprema
  4. Copy each contiguous subrange into its destination Bucket::buf
```

Repair (step 2) is now cheaper than today because it operates on index offsets before
any vector allocation, rather than on already-scattered vectors.

## IPS4o Modifications Required

Two non-breaking additions to `ips4o::detail::Sorter<Cfg>` (in the fork at
`https://github.com/raphinesse/ips4o`, branch `s3q.2`):

### 1. `partitionOnce(begin, end, bucket_start[]) -> {num_buckets, use_equal_buckets}`

Calls `partition<false>(begin, end, bucket_start, 0, 1)` and returns the result
without recursing. The existing `sequential()` recurses into sub-buckets; we want
exactly one level.

### 2. `getSortedSplitters() -> value_type*`

Delegates to `local_.classifier.getSortedSplitters()`. Needed to read back the
splitters chosen by IPS4o's internal sampler after `partitionOnce`, so S3Q can
assign the correct supremum to each new bucket. Note: `ips4o::detail::Classifier`
already exposes `getSortedSplitters()` publicly; this is just a convenience pass-through
on `Sorter`.

Both additions expose nothing that was not already accessible (S3Q already calls
`ips4o::detail::Classifier` directly). They are suitable for upstreaming.

## State Management

**Key constraint:** IPS4o's `LocalData` contains cache-aligned block buffers.
Constructing it dominates runtime up to n = 2²⁶ and reusing it across calls yields a
~3.74× speedup at n = 2²⁴ (measured in the IPS4o paper). It must **not** be
constructed per split.

**Decision:** Store one `Sorter<Ips4oCfg>::LocalData` instance as a member of
`PriorityQueue<Cfg>`, constructed once at queue construction. Each `Level` receives a
reference to it (exactly like `SplitterSampler` today). Each `splitAt()` call
constructs a lightweight `Sorter` on the stack referencing the shared `LocalData`.

Rationale for PQ-level ownership over per-Level ownership:
- The IPS4o config — `kLogBuckets`, `kBlockSizeInBytes`, `kDataAlignment` — is
  independent of level depth, so one `LocalData` type serves all levels identically.
- Buffer storage is ≈ `kMaxBuckets × kBlockSizeInBytes` ≈ 1 MB. Per-level storage
  would waste 3–4× that across a typical queue depth for no benefit.
- `SplitterSampler` is already owned by `PriorityQueue` and shared via reference;
  this follows the same pattern.

The existing `Classifier` member used by `distribute()` is unaffected.

## IPS4o Config for S3Q

A custom `Ips4oCfg` must be provided to `Sorter`. Key parameters:

| Parameter | Value | Reason |
|---|---|---|
| `value_type` | `Cfg::Item` | Items, not just keys — IPS4o sorts full items |
| `less` | Comparator derived from `Cfg::getKey` | Keys extracted from items |
| `kLogBuckets` | `Cfg::kLogMaxDegree + 1` | Match existing S3Q classifier sizing |
| `kBlockSizeInBytes` | default (2 KiB) | Cache-line-friendly; can tune later |
| `kDataAlignment` | default (4 KiB) | Page-aligned buffers |
| `kAllowEqualBuckets` | `false` | S3Q does not use equal buckets |

## Supremum Assignment

After `partitionOnce` returns, sorted splitter $i$ (0-indexed) is the supremum of new
bucket $i$. The last new bucket inherits the old bucket's supremum unchanged. This
mirrors the current logic where `getSplitters()` returns sorted splitters that become
bucket suprema.

## Bad-Split Repair

The underflow threshold and merge-then-recurse repair strategy from the thesis are
preserved unchanged. What changes is timing: repair now adjusts `bucket_start[]`
indices before any destination vector is allocated, which avoids touching cold memory.

Thresholds (unchanged):
- A bucket is underflowing if `2 * size < minBucketSize()`
- Repair: right-to-left merge onto predecessor; if first bucket underflows, merge onto
  successor
- If a merged bucket overflows, `splitAt()` is called recursively (existing behavior)

## Files Changed

| File | Change |
|---|---|
| `extern/ips4o-src/include/ips4o/sequential.hpp` | Add `partitionOnce()`, `getSortedSplitters()`, `resetLocalData()` to `Sorter` |
| `extern/ips4o-src/include/ips4o/ips4o_fwd.hpp` | Forward declarations for the three new `Sorter` methods |
| `include/s3q/splitter.hpp` | **New file**: `BucketSplitter<Cfg>` — owns `SplitCfg`, `SplitSorter`, `BufferStorage`, `LocalData`; exposes `partition()`, `repair()`, `split()` |
| `include/s3q/level.hpp` | Remove IPS4o type aliases; replace `SplitSorter::LocalData&` constructor param with `BucketSplitter<Cfg>&`; rewrite `splitAt()` to call `splitter_.split()` |
| `include/s3q/batched_pq.hpp` | Remove `splitBufStorage_`/`splitLocalDataPtr_`; add `BucketSplitter<Cfg> splitter_`; pass `splitter_` ref to each `Level` ctor |
| `include/s3q/classifier.hpp` | No change |
| `include/s3q/sampling.hpp` | No change (`SplitterSampler` still needed for `distribute()`) |
| `tests/splitter_test.cpp` | **New file**: 9 test cases covering `partition()`, `repair()`, and integration |
| `tests/CMakeLists.txt` | Register `splitter_test` |

## What Does NOT Change

- `distribute()` and its lazy `Classifier` rebuild — untouched
- All `Level` methods other than `splitAt()` — untouched
- `Bucket`, `pq.hpp`, `s3q.hpp`, public API — no change
- Existing tests — should pass without modification

## Verification Plan

1. All existing unit tests in `tests/` pass with no modification
2. Benchmark `bm_test_S3Q_6_15__Wiggle_0_RandomDriver_` shows reduced D1mr/D1mw
   under Callgrind
3. Build produces zero new warnings under the project's `-Werror -pedantic-errors` flags

## Deviations from original design

1. **`BucketSplitter<Cfg>` encapsulation instead of raw `LocalData` reference.**
   The original design proposed storing a raw `SplitSorter::LocalData&` in `Level`.
   The implementation introduces `BucketSplitter<Cfg>` (analogous to `Classifier<Cfg>`)
   which owns both `BufferStorage` and `LocalData` and exposes the three-method split
   interface (`partition`, `repair`, `split`).  This improves separation of concerns
   and makes the split logic independently testable.

2. **`BucketSplitter` owned by `BatchedPriorityQueue`, not `PriorityQueue`.**
   The design originally proposed ownership at `PriorityQueue` level.  In practice
   `BatchedPriorityQueue` is the direct parent of `Level`, so ownership was placed
   there.  The sharing rationale (single instance across all levels) is preserved.

3. **`repair()` also caps the surviving bucket count.**
   IPS4o chooses its own number of buckets based on input size (via `logBuckets(n)`).
   For large buffers this can exceed `Cfg::kSplitFactor`.  The `repair()` method
   therefore accepts an optional `maxBuckets` parameter; when `surviving > maxBuckets`,
   excess buckets are merged from the tail.  This enforces the S3Q fan-out invariant
   (`degree <= kMaxDegree`) without requiring changes to the IPS4o configuration.
   `splitAt()` passes `split_degree` as `maxBuckets`.

4. **IPS4o `SplitCfg` parameters are literal values, not macros.**
   `ips4o/config.hpp` `#undef`s the `IPS4OML_*` macros at the end of the header, so
   they are unavailable when `splitter.hpp` is parsed.  The same default values are
   reproduced as integer literals in `SplitCfg`.

## Future work

- **`SplitterSampler` still held by `Level` / `BatchedPriorityQueue`.**  It is still
  required by the `distribute()` → `Classifier::build(splitters())` path and cannot be
  removed yet.  Once `distribute()` is also migrated to IPS4o-based classification,
  `SplitterSampler` can be removed from the cascade entirely.
  *Location:* `include/s3q/level.hpp` (member `getSplitters`),
  `include/s3q/batched_pq.hpp` (member `sampler_`).

- **`PERF: use compact linked list for buckets` in `level.hpp`.**  `buckets_` is a
  `std::vector`; the `insert` at `splitAt()` is O(n) in degree.  A compact linked list
  or a deque would reduce this cost.
  *Location:* `include/s3q/level.hpp`, `fixOverflowingBuckets` comment.

- **IPS4o config tuning.**  The `SplitCfg` in `splitter.hpp` uses IPS4o's default
  parameters.  `kBaseCaseSize`, `kBlockSize`, and `kDataAlignment` have not been
  benchmarked for the S3Q use case and may benefit from tuning.
  *Location:* `include/s3q/splitter.hpp`, `SplitCfg` alias.

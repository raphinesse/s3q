# S3Q Codebase and Algorithm Knowledge Base

This document captures everything relevant learned about the S3Q codebase, the IPS4o
library it depends on, and the theoretical underpinnings of both. It is intended to
help future AI agents continue improving S3Q without needing to re-derive this
knowledge from scratch.

---

## Repository Layout

```
include/s3q/       — header-only S3Q implementation
  s3q.hpp          — public entry point, re-exports PriorityQueue
  pq.hpp           — PriorityQueue<Cfg>: top-level multi-level structure
  level.hpp        — Level<Cfg>: one level of the cascade; contains splitAt()
  bucket.hpp       — Bucket<Cfg>: a Key supremum + std::vector<Item> buffer
  classifier.hpp   — Classifier<Cfg>: thin wrapper over ips4o::detail::Classifier
  sampling.hpp     — SplitterSampler: samples + sorts splitters from a key range
  config.hpp       — DefaultCfg + ExtendedCfg<Base>: all tuning knobs
  util.hpp         — log2_floor/ceil, num_cast, ssize, append helpers
  heap.hpp         — small heap used internally
  batched_pq.hpp   — batched push/pop interface

extern/ips4o-src/  — IPS4o fork (branch s3q.2, github.com/raphinesse/ips4o)
  include/ips4o/
    classifier.hpp     — branch-free classifier tree (already used by S3Q directly)
    partitioning.hpp   — Sorter<Cfg>::partition<kIsParallel>(): the core routine
    sequential.hpp     — Sorter<Cfg>::sequential(): recursive entry point
    memory.hpp         — LocalData, SharedData, BufferStorage, AlignedPtr
    config.hpp         — ips4o::Config<...>: all IPS4o tuning parameters
    sampling.hpp       — IPS4o's internal splitter sampler
    buffers.hpp        — Block-aligned write buffer management
    block_permutation.hpp — permuteBlocks(): the in-place rearrangement pass
    cleanup_margins.hpp   — writeMargins(): flushes partial buffer blocks back
    local_classification.hpp — sequentialClassification()

tests/             — CTest-registered unit tests
benchmarks/        — CMake-generated benchmark binaries
scripts/           — run-benchmarks, results_to_tsv.py
docs/              — papers, design notes
```

---

## Configuration System

`DefaultCfg` defines all user-facing knobs. `ExtendedCfg<Base>` derives computed
constants from them. `PriorityQueue<Cfg>` uses `ExtendedCfg<Cfg>`.

Key constants (from `ExtendedCfg`):

| Constant | Meaning |
|---|---|
| `kBufBaseSize` | Maximum bucket size at level 0; ≈ L1 / (4 × sizeof(Item)) |
| `kLogMaxDegree` | log₂ of max fan-out per level; default 6 → kMaxDegree = 64 |
| `kMaxDegree` | 1 << kLogMaxDegree |
| `kMinDegree` | kMaxDegree >> 1 |
| `kSplitFactor` | 1 << (kLogMaxDegree >> 1); default 8 (√64) |
| `kGrowthRate` | kMaxDegree - kMinDegree; bucket size multiplier between levels |

At each level $i$, `kMaxBucketSize = kBufBaseSize × kGrowthRate^i`.

---

## Data Flow: PriorityQueue Operations

### push
Items accumulate in the last bucket of level 0. When level 0 overflows (its last
bucket exceeds `kMaxBucketSize`), `flushMaxBufInto(level1)` moves excess items down.
Each level's `insert()` calls `distribute()` to scatter into existing buckets, then
`fixOverflowingBuckets()` which calls `splitAt()` for any bucket that is too large.

### pop
`delMin()` extracts the first bucket from level 0. When level 0 has too few buckets
(`degree == kMinDegree + 1`), `refillFrom(level1)` steals the minimum bucket from
level 1 and splits it into the level-0 fan-out.

---

## Level<Cfg> Internals

`buckets_` is a `std::vector<Bucket>` kept in key-sorted order. The last bucket
("max-buf") is special: it has no fixed upper bound on size and acts as a holding
area. All other buckets are "regular" and must satisfy size constraints.

`classifier_` is lazily rebuilt: it is invalidated whenever `buckets_` changes
structure (split, join, insert), and rebuilt on demand before the next `distribute()`
call. Rebuilding is amortized because S3Q always classifies Ω(M) items per call.

`splitAt(idx, split_degree)`:
1. Retires tail buckets into max-buf if needed to make room for split_degree new ones
2. Samples splitters via `getSplitters` (the `SplitterSampler` functor stored as a ref)
3. Inserts empty `Bucket` entries with the new suprema into `buckets_`
4. Builds a temporary local `Classifier` over the new splitters
5. Classifies all items and scatters them (`push_back`) into the new bucket vectors
6. Repairs underflowing buckets (merges onto neighbor, recursively splits if overflow)

---

## Classifier<Cfg> (S3Q's wrapper)

Wraps `ips4o::detail::Classifier<Ips4oCfg>`. Exposes:
- `build(sorted_keys)`: pads to next power-of-two, copies into IPS4o's sorted storage,
  calls `classifier_.build(log_buckets)`
- `classify(subjects, yield)`: calls IPS4o's unrolled `classify<false>(begin, end, yield)`
- `valid()` / `invalidate()`

The `Ips4oCfg` inside uses `Cfg::Key` as `value_type`, `Cfg::BucketIdx` as
`bucket_type`, and sets `kLogBuckets = Cfg::kLogMaxDegree + 1`.

---

## IPS4o Architecture

### Sorter<Cfg>
The central class. Holds references to `LocalData` (owns the buffers and classifier)
and optionally `SharedData` (for parallel use). Never constructed directly by users;
created inside `ips4o::sort()` or `ips4o::SequentialSorter`.

Key members set per `partition()` call:
- `classifier_` — pointer to local or shared classifier
- `bucket_start_` — caller-provided array of size kMaxBuckets+1
- `bucket_pointers_` — per-bucket read/write head tracking
- `begin_`, `end_` — range being partitioned
- `num_buckets_` — chosen by sampler

### partition<kIsParallel>(begin, end, bucket_start[], my_id, num_threads)
The core routine. Sequentially:
1. `buildClassifier(begin, end, local_.classifier)` — samples, sorts splitters,
   builds classifier tree; returns `{num_buckets, use_equal_buckets}`
2. `sequentialClassification(use_equal_buckets)` — reads items left-to-right, writes
   into block-aligned local buffers; when a buffer fills, flushes the full block into
   the array
3. `permuteBlocks<false, false>()` — moves full blocks into correct bucket positions
   using a cycle-following algorithm (in-place, O(n) moves)
4. `writeMargins()` — flushes partial buffer blocks (head/tail items near bucket
   boundaries) back into the array

After step 4, all elements are in the original array, correctly partitioned. Bucket $i$
occupies `[begin + bucket_start[i], begin + bucket_start[i+1])`. Boundaries are
element-aligned, not block-aligned.

### LocalData
Owns:
- `Classifier local_.classifier` — the classifier tree + sorted splitter storage
- `BucketPointers local_.bucket_pointers` — read/write heads per bucket
- `Buffers` — block-aligned write staging buffers (the expensive part to allocate)

**Construction cost dominates up to n ≈ 2²⁶.** Reuse across calls gives ~3.74×
speedup at n = 2²⁴. Must be constructed once and reused.

### Classifier (IPS4o's)
A perfectly balanced implicit binary tree of splitters stored in BFS order. Supports
branch-free classification via a bit-manipulation walk. The unrolled `classify(begin,
end, yield)` is the same code S3Q already calls for `distribute()`.

`getSortedSplitters()` returns a pointer to the sorted splitter array (used both as
input to `build()` and as readable output after building). After `buildClassifier()`
runs inside `partition()`, sorted splitter $i$ (0-indexed) is the supremum of new
bucket $i$.

---

## Theoretical Guarantees (from Papers)

### S3Q (thesis)

- A split is "good" if each resulting bucket $i$ has size $r_i \in [1/2, 4/3]$ times
  the target size $N/d$.
- A regular bucket at level $i$ (when degree > 1) must have size ≥ `kMaxBucketSize /
  kSplitFactor / 2` = `minBucketSize() / 2`.
- The current bad-split repair is: single right-to-left pass merging any underflowing
  bucket onto its predecessor (or successor for the first bucket). If a merged bucket
  overflows, `splitAt()` recurses. The thesis notes that theoretically a full sort
  could be used as fallback, but the implementation just merges.
- The choice to scatter into separate `std::vector` instances was an implementation
  convenience, explicitly acknowledged as a limitation. The thesis's "Future Work"
  section recommends adopting IPS4o's in-place partitioning to avoid copying overhead
  and to enable cache-aligned contiguous write buffers.
- The classifier is rebuilt lazily and its cost is amortized because S3Q always
  classifies Ω(M) items (where M = L1 cache size) per call.

### IPS4o (paper)

- After the full `partition()` call (classification + block permutation + cleanup),
  all elements are back in the input array and correctly partitioned.
- Block-aligned buffers are staging areas flushed back during cleanup. No elements
  remain outside the array after `partition()` returns.
- Base case threshold: tasks with ≤ 2n₀ elements skip partitioning and use insertion
  sort. Default n₀ = 16. Configurable via `kBaseCaseSize` and `kBaseCaseMultiplier`.
- Bucket boundaries after partitioning are element-aligned only, not block-aligned.
  The cleanup phase exists to handle items straddling block boundaries.
- `LocalData` initialization dominates runtime below n = 2²⁶. The API deliberately
  separates construction from sorting to allow object reuse. This is critical for
  any use that calls `partition()` repeatedly on small inputs (like S3Q bucket splits).

---

## IPS4o Fork (branch s3q.2) Notes

The fork at `github.com/raphinesse/ips4o` branch `s3q.2` is what S3Q pins. It already
exposes `ips4o::detail::Classifier` publicly (S3Q's `classifier.hpp` depends on it).
The `Sorter` class is in `detail` but its headers are included transitively.

CMake fetches this via `FetchContent` in `extern/CMakeLists.txt`. The local clone
checked out for development lives at `extern/ips4o-src/`.

---

## Important Non-Obvious Interactions

1. **`getSplitters` is a reference** stored in `Level`. The `SplitterSampler` lives in
   `PriorityQueue` and is shared across all levels. Each level holds a ref to it.
   After replacing `getSplitters()` calls in `splitAt()` with IPS4o's internal
   sampler, `SplitterSampler` will still be needed for any remaining use but is
   removable from the split hot path. The IPS4o `LocalData` should be owned by
   `PriorityQueue` and shared across all levels via reference, exactly like
   `SplitterSampler` — the IPS4o config does not vary between levels and the
   buffer storage (~1 MB) makes per-level duplication wasteful.

2. **`shrinkToDegree()` is called inside `splitAt()`** before the actual split, to
   retire tail buckets into the max-buf and make room for split_degree new ones. This
   happens before the new `partition()` call would occur. The flow is preserved.

3. **`fixOverflowingBuckets()` calls `splitAt()` recursively.** After the repair pass,
   newly merged buckets may overflow and trigger another split. This recursion is
   bounded by the structure invariants.

4. **`classifier_.invalidate()` must still be called** whenever `buckets_` is
   structurally modified by `splitAt()`, to ensure `distribute()` rebuilds before
   next use.

5. **The `is_last_` flag** controls whether the max-buf is subject to overflow checks.
   It is set/cleared by `flushMaxBufInto` and `refillFrom`. `splitAt()` does not
   touch it.

6. **`kSplitFactor` (default 8)** is the minimum split degree. `splitAt()` accepts a
   `split_degree` argument that can be larger (used in `refillFrom` for non-standard
   splits). The IPS4o-based implementation must support arbitrary split degrees, not
   just powers of two.

---

## Build System Notes

- All headers are under `include/s3q/` and `extern/*/include/`. No `.cpp` files in
  the library itself (header-only).
- Benchmarks are code-generated from `.cpp.in` templates via CMake `configure_file`.
- The benchmark target name encoding: `bm_test_S3Q_6_15__Wiggle_0_RandomDriver_`
  encodes subject (`S3Q<6,15>`), workload (`Wiggle<0,RandomDriver>`).
- Tests are in `tests/` and registered with CTest. Build target `all_tests` builds
  both unit tests and benchmark tests.
- Compiler flags include `-Werror -pedantic-errors -Wconversion -Wsign-conversion`.
  Any new code must be warning-clean.
- IPS4o is fetched as a shallow clone at build time; for development, it was manually
  cloned to `extern/ips4o-src/`.

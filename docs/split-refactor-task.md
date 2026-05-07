## Task: Optimize S3Q Bucket Splitting using $IPS^4o$ In-place Partitioning

### Context
You are tasked with optimizing the bucket-splitting mechanism of the **SuperScalarSampleQueue (S3Q)**. Currently, when a bucket overflows, the backend uses a classifier tree to redistribute items into $\alpha$ new vectors. This is inefficient for handling "bad splits" and lacks the cache-line alignment benefits of **$IPS^4o$ (In-place Parallel Super Scalar Samplesort)**. According to Callgrind, `s3q::detail::Level<>::splitAt(long, long)` is responsible for around 40% of all L1 cache misses.

### Objectives
Your goal is to replace the current heavy-handed classifier-based split with the **in-place partitioning logic** from $IPS^4o$ to improve cache efficiency and simplify recovery from unbalanced splits.

### Step 1: Research & Discovery
1.  **Read Papers:** Review the $S3Q$ and $IPS^4o$ papers (located in `/docs`) to understand the performance-critical decisions regarding branch prediction and cache-miss minimization.
2.  **Analyze Codebase:**
    * Locate the bucket-splitting logic in the `SuperScalarSampleQ` source.
    * Analyze the `IPS4o` implementation. Identify the specific partitioning routines (sequential and parallel) and their internal dependencies.
3.  **Identify State:** Determine what state is stored within an $IPS^4o$ instance and evaluate if instances should be reused at the S3Q level or recreated per split.

### Step 2: High-Level Refactoring Plan
Before writing code, present a design proposal covering:
* **Integration Strategy:** How to invoke $IPS^4o$ partitioning on the contiguous memory of an S3Q bucket.
* **Library Modifications:** Any necessary changes to $IPS^4o$ (e.g., exposing internals from the `detail` namespace).
* **Fallback Handling:** How to leverage the in-place state to continue a full sort if a split is deemed "bad," avoiding the need to concatenate vectors.
* **State Management:** Where the $IPS^4o$ instance will live and potential optimizations for state reuse.

### Step 3: Implementation & Guardrails
* **Minimal Intrusion:** Do **not** create a permanent hard fork of $IPS^4o$. Any changes to $IPS^4o$ should be backwards-compatible, minimal, and ideally suitable for upstreaming.
* **Performance:** Maintain the strict cache-line alignment and memory efficiency standards of the original algorithms.
* **Transition:** Ensure the partitioned contiguous memory is efficiently copied into the $\alpha$ destination vectors after a successful split.

### Step 4: Verification
* The modified system must pass all existing unit and integration tests.
* The code must build without warnings under the project's standard configuration.

***

**Constraint:** Do not proceed with implementation until the High-Level Refactoring Plan is approved.
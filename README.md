# Superscalar Sample Queue (S³Q)

Superscalar Sample Queue is a comparison-based, cache-efficient priority queue that avoids branch mispredictions. It supports operations _push_ and _pop_ in amortized expected time O(log _N_) and an amortized expected number of O(1/_B_ log _M/B_ _N/M_) memory accesses on queues of size _N_, where _M_ is the cache size and _B_ the cache line size.

This repository contains a header-only template implementation of S³Q in portable C++17 as well as benchmarks comparing S³Q to other priority queues.

## Usage

To use S³Q you have to add `include/` to your include paths and ensure its dependencies (`ips4o`, `range-v3` and `xoshiro`) are available too. The required versions of these dependencies can be found in `extern/CMakeLists.txt`.

After the include path has been set up, S³Q can be used as follows:

```cpp
#include <s3q/s3q.hpp>
#include <cassert>

struct Cfg : s3q::DefaultCfg {
    // Specify the data type stored by the PQ
    // In this example we only store unsigned keys
    // If `Item` has a member `key` it will be used as key
    // You can also provide a custom GetKey functor
    // see s3q/config.hpp for more details & parameters
    using Item = unsigned;
};

int main() {
    s3q::PriorityQueue<Cfg> pq;
    assert(pq.empty());

    for (unsigned n = 100; n > 0; --n)
        pq.push(n);

    assert(pq.size() == 100);
    assert(pq.top() == 1);
    pq.pop();
    assert(pq.size() == 99);
    assert(pq.top() == 2);
}

```

## Running tests and benchmarks

First, if you want to collect `perf` events during benchmarks, make sure that you have the necessary privileges – collection will be disabled during configuration if you don't. To gain privileges on Ubuntu, run
```sh
echo 0 | sudo tee /proc/sys/kernel/kptr_restrict > /dev/null
echo 0 | sudo tee /proc/sys/kernel/perf_event_paranoid > /dev/null
```

### Configure
```sh
mkdir build && cd build
# Use -DCMAKE_BUILD_TYPE=Debug for development & testing
cmake -DCMAKE_BUILD_TYPE=Release ..
```

### Build

Run the following commands inside the `build` directory.

To build all benchmarks
```sh
cmake --build . -j $(nproc)
```

To build all tests
```sh
cmake --build . -j $(nproc) --target all_tests
```

All binaries will be written to `build/bin`.

### Run
Run the following commands inside the `build` directory.

To run all tests:
```sh
ctest --output-on-failure -j $(nproc)
```

To run the benchmarks, simply execute them:
```sh
./bin/benchmark_S3Q_6_15__Wiggle_0_RandomDriver_ | tee results.txt
```
The output can be transformed to TSV by `scripts/results_to_tsv.py`:
```sh
../scripts/results_to_tsv.py < results.txt > results.tsv
```

## License

MIT © 2021 Raphael von der Grün

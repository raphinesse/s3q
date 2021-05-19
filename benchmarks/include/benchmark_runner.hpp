#pragma once

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

#include <tlx/timestamp.hpp>

#include "perf_count.hpp"

template <class Benchmark>
class BenchmarkRunner {
    using Subject = typename Benchmark::subject_type;

    const size_t min_items, max_items;

    // The number of items to be processed in one benchmark batch
    size_t batch_size = min_items;

    // The maximum amount of events in a group seems to be 3 on AMD K10. But
    // since related metrics should be in the same group, we have to manually
    // create groups of two
    PerfCount perf_count_{{
#ifdef BM_COLLECT_PERF_EVENTS
        {
            PERF_EVENT_HW(INSTRUCTIONS),
            PERF_EVENT_HW(CPU_CYCLES),
            PERF_EVENT_HW(BRANCH_MISSES),
        },
        {
            PERF_EVENT_CACHE(L1D, READ, MISS),
            PERF_EVENT_CACHE(LL, READ, MISS),
            PERF_EVENT_CACHE(DTLB, READ, MISS),
        },
#endif
    }};

    struct Result {
        const size_t run_size, num_runs;
        const double time;

        friend std::ostream &operator<<(std::ostream &os, const Result &r) {
            // clang-format off
            return os << "RESULT"
                << " container=" << Subject::name()
                << " op=" << Benchmark::name()
                << " items=" << r.run_size
                << " repeat=" << r.num_runs
                << std::fixed << std::setprecision(10)
                << " time_total=" << r.time
                << " time=" << r.time / static_cast<double>(r.num_runs);
            // clang-format on
        }
    };

    // Repeat benchmark runs of given size until enough time elapsed
    Result run_until_stable(size_t run_size) {
        double time;
        while ((time = run_batch(run_size)) < 1.0) {
            batch_size *= 2;
        }
        return {run_size, batch_size / run_size, time};
    }

    // Run a batch of benchmark runs of given size and return total time
    double run_batch(size_t run_size) {
        size_t num_runs = batch_size / run_size;
        Benchmark benchmark;

        double ts1 = tlx::timestamp();
        perf_count_.reset();
        perf_count_.enable();
        for (size_t r = 0; r < num_runs; ++r) {
            benchmark.run(run_size);
        }
        perf_count_.disable();
        double ts2 = tlx::timestamp();

        return ts2 - ts1;
    }

public:
    // *-member-init is a false positive, see:
    // https://bugs.llvm.org/show_bug.cgi?id=37902
    // NOLINTNEXTLINE(hicpp-member-init, cppcoreguidelines-pro-type-member-init)
    BenchmarkRunner() : BenchmarkRunner(125, 1024000 * 128) {}

    BenchmarkRunner(size_t min_items, size_t max_items)
        : min_items(min_items), max_items(max_items) {}

    void run_benchmark() {
        std::cout << "Benchmark " << Subject::name() << " " << Benchmark::name()
                  << " " << min_items << ".." << max_items << "\n";

        for (size_t items = min_items; items <= max_items; items *= 2) {
            auto result = run_until_stable(items);
            std::cout << result;

            for (auto &&[name, value] : perf_count_.get_results()) {
                std::cout << " " << name << "=" << value;
            }

            std::cout << std::endl;
        }
    }
};

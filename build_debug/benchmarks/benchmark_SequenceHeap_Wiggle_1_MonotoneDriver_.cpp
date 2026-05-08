#include "benchmark_runner.hpp"
#include "workloads.hpp"

#include "subjects/SequenceHeap.hpp"

template <typename T> struct Subject : SequenceHeap<T> {
    static auto name() { return "SequenceHeap"; }
};

using Benchmark = Wiggle<1,MonotoneDriver>::type<Subject>;

int main() {
    BenchmarkRunner<Benchmark> runner;
    runner.run_benchmark();
}

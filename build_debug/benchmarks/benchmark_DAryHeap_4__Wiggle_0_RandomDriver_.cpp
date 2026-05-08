#include "benchmark_runner.hpp"
#include "workloads.hpp"

#include "subjects/DAryHeap.hpp"

template <typename T> struct Subject : DAryHeap<4>::type<T> {
    static auto name() { return "DAryHeap<4>"; }
};

using Benchmark = Wiggle<0,RandomDriver>::type<Subject>;

int main() {
    BenchmarkRunner<Benchmark> runner;
    runner.run_benchmark();
}

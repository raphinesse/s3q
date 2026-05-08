#include "benchmark_runner.hpp"
#include "workloads.hpp"

#include "subjects/StdQueue.hpp"

template <typename T> struct Subject : StdQueue<T> {
    static auto name() { return "StdQueue"; }
};

using Benchmark = Wiggle<1,RandomDriver>::type<Subject>;

int main() {
    BenchmarkRunner<Benchmark> runner;
    runner.run_benchmark();
}

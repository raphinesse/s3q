#include "benchmark_runner.hpp"
#include "workloads.hpp"

#include "subjects/S3Q.hpp"

template <typename T> struct Subject : S3Q<6,15>::type<T> {
    static auto name() { return "S3Q<6,15>"; }
};

using Benchmark = Wiggle<1,RandomDriver>::type<Subject>;

int main() {
    BenchmarkRunner<Benchmark> runner;
    runner.run_benchmark();
}

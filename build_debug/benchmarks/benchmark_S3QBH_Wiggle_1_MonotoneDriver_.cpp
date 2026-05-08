#include "benchmark_runner.hpp"
#include "workloads.hpp"

#include "subjects/S3QBH.hpp"

template <typename T> struct Subject : S3QBH<T> {
    static auto name() { return "S3QBH"; }
};

using Benchmark = Wiggle<1,MonotoneDriver>::type<Subject>;

int main() {
    BenchmarkRunner<Benchmark> runner;
    runner.run_benchmark();
}

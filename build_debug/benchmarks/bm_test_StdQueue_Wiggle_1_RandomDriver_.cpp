#include "workloads.hpp"
#include "subjects/StdQueue.hpp"

using Test = Wiggle<1,RandomDriver>::type<StdQueue>;

int main() {
    Test().run(1024000);
}

#include "workloads.hpp"
#include "subjects/StdQueue.hpp"

using Test = Wiggle<0,RandomDriver>::type<StdQueue>;

int main() {
    Test().run(1024000);
}

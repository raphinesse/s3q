#include "workloads.hpp"
#include "subjects/DAryHeap.hpp"

using Test = Wiggle<0,RandomDriver>::type<DAryHeap<4>::type>;

int main() {
    Test().run(1024000);
}

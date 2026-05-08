#include "workloads.hpp"
#include "subjects/SequenceHeap.hpp"

using Test = Wiggle<1,RandomDriver>::type<SequenceHeap>;

int main() {
    Test().run(1024000);
}

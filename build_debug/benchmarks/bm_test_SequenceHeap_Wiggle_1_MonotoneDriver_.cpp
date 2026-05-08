#include "workloads.hpp"
#include "subjects/SequenceHeap.hpp"

using Test = Wiggle<1,MonotoneDriver>::type<SequenceHeap>;

int main() {
    Test().run(1024000);
}

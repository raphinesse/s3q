#include "workloads.hpp"
#include "subjects/DAryHeap.hpp"

using Test = Wiggle<1,MonotoneDriver>::type<DAryHeap<4>::type>;

int main() {
    Test().run(1024000);
}

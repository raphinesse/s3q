#include "workloads.hpp"
#include "subjects/S3QBH.hpp"

using Test = Wiggle<1,MonotoneDriver>::type<S3QBH>;

int main() {
    Test().run(1024000);
}

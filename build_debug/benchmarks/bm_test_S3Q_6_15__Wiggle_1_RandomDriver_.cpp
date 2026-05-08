#include "workloads.hpp"
#include "subjects/S3Q.hpp"

using Test = Wiggle<1,RandomDriver>::type<S3Q<6,15>::type>;

int main() {
    Test().run(1024000);
}

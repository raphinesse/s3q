#include "workloads.hpp"
#include "subjects/S3QBH.hpp"

using Test = Wiggle<0,RandomDriver>::type<S3QBH>;

int main() {
    Test().run(1024000);
}

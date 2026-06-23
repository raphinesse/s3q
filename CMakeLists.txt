cmake_minimum_required(VERSION 3.13.4)
project(s3q VERSION 0.4.2)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Place all built binaries under <BUILD_DIR>/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add 3rd party libraries
add_subdirectory(extern)

# bluntly enable warnings for all following targets
add_compile_options(
    -Werror -pedantic-errors
    -Wall -Wextra
    -Wconversion -Wsign-conversion
)

# Add library target for S³Q
add_library(s3q INTERFACE)
target_compile_features(s3q
    INTERFACE cxx_std_17)
target_include_directories(s3q
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(s3q
    INTERFACE ips4o range-v3 xoshiro)

# Add targets for test and benchmark binaries
enable_testing()
add_subdirectory(tests)
add_subdirectory(benchmarks)

# Add target to build all tests
add_custom_target(all_tests)
add_dependencies(all_tests bm_tests s3q_tests)

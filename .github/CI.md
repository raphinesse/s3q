# CI Pipeline Documentation

This document describes the continuous integration (CI) setup for the s3q project.

## Overview

The CI pipeline uses GitHub Actions to automatically build and test the project across multiple compiler and platform combinations to ensure maximum compatibility.

## Matrix Coverage

The CI pipeline tests **72 different combinations** covering:

### Operating Systems
- Ubuntu 20.04 LTS
- Ubuntu 22.04 LTS  
- Ubuntu 24.04 LTS

### Compilers
- **GCC versions**: 9, 10, 11, 12, 13, 14
- **Clang versions**: 10, 11, 12, 13, 14, 15, 16, 17, 18

### Build Types
- Debug
- Release

## Exclusions

Some combinations are excluded because the compilers are not available on specific Ubuntu versions:

- **Ubuntu 20.04**: Excludes GCC 13-14 and Clang 15-18
- **Ubuntu 22.04**: Excludes GCC 14 and Clang 17-18

## Pipeline Steps

For each combination, the CI pipeline:

1. **Sets up the environment**: Installs the specified compiler version
2. **Configures CMake**: Uses the selected compiler and build type
3. **Builds the project**: Compiles all tests and benchmarks
4. **Runs tests**: Executes the test suite with CTest
5. **Uploads artifacts**: On failure, uploads build logs for debugging

## LLVM Repository Setup

For newer Clang versions (15+), the pipeline automatically adds the official LLVM apt repositories to access the latest compiler versions.

## Monitoring

- The CI status is visible via the badge in the README
- Individual job results show which compiler/platform combinations pass or fail
- Build artifacts are available for failed jobs to aid in debugging

This comprehensive testing ensures the s3q library works correctly across the widest possible range of modern C++17 compilers and platforms.
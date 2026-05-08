# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/runner/work/s3q/s3q/build_debug/_deps/tlx-src")
  file(MAKE_DIRECTORY "/home/runner/work/s3q/s3q/build_debug/_deps/tlx-src")
endif()
file(MAKE_DIRECTORY
  "/home/runner/work/s3q/s3q/build_debug/_deps/tlx-build"
  "/home/runner/work/s3q/s3q/build_debug/_deps/tlx-subbuild/tlx-populate-prefix"
  "/home/runner/work/s3q/s3q/build_debug/_deps/tlx-subbuild/tlx-populate-prefix/tmp"
  "/home/runner/work/s3q/s3q/build_debug/_deps/tlx-subbuild/tlx-populate-prefix/src/tlx-populate-stamp"
  "/home/runner/work/s3q/s3q/build_debug/_deps/tlx-subbuild/tlx-populate-prefix/src"
  "/home/runner/work/s3q/s3q/build_debug/_deps/tlx-subbuild/tlx-populate-prefix/src/tlx-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/runner/work/s3q/s3q/build_debug/_deps/tlx-subbuild/tlx-populate-prefix/src/tlx-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/runner/work/s3q/s3q/build_debug/_deps/tlx-subbuild/tlx-populate-prefix/src/tlx-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()

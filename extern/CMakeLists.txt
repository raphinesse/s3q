include(FetchContent)

macro(fetch)
    FetchContent_Declare(${ARGV})
    FetchContent_GetProperties(${ARGV0})
    if(NOT ${ARGV0}_POPULATED)
        FetchContent_Populate(${ARGV0})
    endif()
endmacro()

fetch(ips4o
    GIT_REPOSITORY  https://github.com/raphinesse/ips4o.git
    GIT_TAG         s3q
    GIT_SHALLOW     True)
add_library(ips4o INTERFACE)
target_include_directories(ips4o
    SYSTEM INTERFACE ${ips4o_SOURCE_DIR}/include)

# There is no current stable release, so we pin to a SHA.
# Unfortunately that means we cannot do a shallow clone.
fetch(range-v3
    GIT_REPOSITORY  https://github.com/ericniebler/range-v3.git
    GIT_TAG         6f3d0e2af9ae)
add_library(range-v3 INTERFACE)
target_include_directories(range-v3
    SYSTEM INTERFACE ${range-v3_SOURCE_DIR}/include)

# Adds subset of tlx as library
fetch(tlx
    GIT_REPOSITORY  https://github.com/tlx/tlx.git
    GIT_TAG         v0.5.20200222
    GIT_SHALLOW     True)
set(TLX_MINI_SOURCES die/core.cpp logger/core.cpp timestamp.cpp)
list(TRANSFORM TLX_MINI_SOURCES PREPEND "${tlx_SOURCE_DIR}/tlx/")
add_library(tlx-mini STATIC ${TLX_MINI_SOURCES})
target_include_directories(tlx-mini
    SYSTEM PUBLIC ${tlx_SOURCE_DIR})

# Fast random number generation
fetch(xoshiro
    GIT_REPOSITORY  https://github.com/Reputeless/Xoshiro-cpp.git
    GIT_TAG         v1.1
    GIT_SHALLOW     True)
add_library(xoshiro INTERFACE)
target_include_directories(xoshiro
    SYSTEM INTERFACE ${xoshiro_SOURCE_DIR})

# Add Sequence Heap library
fetch(spq
    GIT_REPOSITORY  https://github.com/raphinesse/SequenceHeap.git
    GIT_TAG         s3q/basic
    GIT_SHALLOW     True)
add_library(spq INTERFACE)
target_include_directories(spq
    SYSTEM INTERFACE ${spq_SOURCE_DIR})

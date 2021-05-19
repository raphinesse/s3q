# check if we are perf-paranoid
set(BM_PERF_PARANOID_FILE /proc/sys/kernel/perf_event_paranoid)
if (EXISTS ${BM_PERF_PARANOID_FILE})
    file(READ ${BM_PERF_PARANOID_FILE} BM_PERF_PARANOID)
    string(STRIP "${BM_PERF_PARANOID}" BM_PERF_PARANOID)
else()
    set(BM_PERF_PARANOID 2)
endif()

function(add_benchmark_target TYPE BM_SUBJECT BM_WORKLOAD)
    string(MAKE_C_IDENTIFIER
        ${TYPE}_${BM_SUBJECT}_${BM_WORKLOAD} TARGET_NAME)
    string(REPLACE "__type" "" TARGET_NAME ${TARGET_NAME})

    # Save the subject's name without ::type
    string(REPLACE "::type" "" BM_SUBJECT_NAME ${BM_SUBJECT})

    # Save the subject's name without arguments
    string(REGEX MATCH "^[^<]+" BM_SUBJECT_BASE_NAME ${BM_SUBJECT})

    set(BM_SRC_FILE ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.cpp)
    configure_file(${TYPE}.cpp.in ${BM_SRC_FILE})
    add_executable(${TARGET_NAME} ${BM_SRC_FILE})

    # For tests: register test, add to `bm_tests` and exclude from `all`
    if (TYPE STREQUAL bm_test)
        add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
        add_dependencies(bm_tests ${TARGET_NAME})
        set_target_properties(${TARGET_NAME}
            PROPERTIES EXCLUDE_FROM_ALL TRUE)
    elseif(BM_PERF_PARANOID LESS 2)
        # collect perf events during benchmarks if available
        target_compile_definitions(${TARGET_NAME}
            PUBLIC BM_COLLECT_PERF_EVENTS)
    endif()

    # Add SOURCE_DIR to include dirs so we can find our headers from BINARY_DIR
    target_include_directories(${TARGET_NAME}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

    target_link_libraries(${TARGET_NAME}
        PRIVATE tlx-mini ${ARGN})
endfunction()

function(add_benchmark_subject SUBJECT_NAME)
    foreach(WORKLOAD_NAME
        Wiggle<0,RandomDriver>::type
        Wiggle<1,RandomDriver>::type
        Wiggle<1,MonotoneDriver>::type
    )
        add_benchmark_target(bm_test ${SUBJECT_NAME} ${WORKLOAD_NAME} ${ARGN})
        add_benchmark_target(benchmark ${SUBJECT_NAME} ${WORKLOAD_NAME} ${ARGN})
    endforeach()
endfunction()

# Target to build all benchmark tests
add_custom_target(bm_tests)

add_benchmark_subject(S3Q<6,15>::type s3q)
add_benchmark_subject(S3QBH s3q)
add_benchmark_subject(StdQueue)
add_benchmark_subject(SequenceHeap spq)
add_benchmark_subject(DAryHeap<4>::type)

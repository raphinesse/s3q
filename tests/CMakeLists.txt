add_custom_target(s3q_tests)

foreach(SRC_NAME
    pq_test
    batched_pq_test
    classifier_test
    monotone_pq_test
)
    set(TEST_NAME s3q_${SRC_NAME})
    add_executable(${TEST_NAME} EXCLUDE_FROM_ALL ${SRC_NAME}.cpp)
    target_link_libraries(${TEST_NAME} PRIVATE s3q PRIVATE tlx-mini)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    add_dependencies(s3q_tests ${TEST_NAME})
endforeach()

# Diagnostic variant of the monotone test: compiled with S3Q_SPLIT_DIAG so the
# split function prints human-readable key-distribution snapshots to stderr.
# Used only to capture the "before fix" and "after fix" traces for the PR.
add_executable(s3q_monotone_pq_test_diag EXCLUDE_FROM_ALL monotone_pq_test.cpp)
target_link_libraries(s3q_monotone_pq_test_diag PRIVATE s3q PRIVATE tlx-mini)
target_compile_definitions(s3q_monotone_pq_test_diag PRIVATE S3Q_SPLIT_DIAG)

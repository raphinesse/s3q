add_custom_target(s3q_tests)

foreach(SRC_NAME
    pq_test
    batched_pq_test
    classifier_test
)
    set(TEST_NAME s3q_${SRC_NAME})
    add_executable(${TEST_NAME} EXCLUDE_FROM_ALL ${SRC_NAME}.cpp)
    target_link_libraries(${TEST_NAME} PRIVATE s3q PRIVATE tlx-mini)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    add_dependencies(s3q_tests ${TEST_NAME})
endforeach()

# TODO: raphinesse/s3q#5 - fix library so this test passes again
set_tests_properties(s3q_batched_pq_test
                     PROPERTIES DISABLED TRUE)

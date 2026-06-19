include(GoogleTest)

SET(SMALL_CANON_TESTS
        adj_matrix
        adj_matrix_storage
        bitspan
        canon
        coloring
        parser
        refine
        simd_sort
        simd
)

function(add_smallcanon_test name)
    add_executable(smallcanon_test_${name} ${name}.cpp)
    target_link_libraries(smallcanon_test_${name}
            PRIVATE
            smallcanon::smallcanon
            GTest::gtest_main
    )
    target_compile_definitions(smallcanon_test_${name}
            PRIVATE
            SMALLCANON_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
    )

    gtest_discover_tests(smallcanon_test_${name})
endfunction()

if (INDIV_TESTS)
    foreach (SRC IN LISTS SMALL_CANON_TESTS)
        add_smallcanon_test(SRC)
    endforeach ()
else ()
    add_executable(smallcanon_test)
    foreach (SRC IN LISTS SMALL_CANON_TESTS)
        target_sources(smallcanon_test PRIVATE ${SRC}.cpp)
    endforeach ()


    target_link_libraries(smallcanon_test
            PRIVATE
            smallcanon::smallcanon
            GTest::gtest_main
    )
    target_compile_definitions(smallcanon_test
            PRIVATE
            SMALLCANON_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
    )

    gtest_discover_tests(smallcanon_test)
endif ()

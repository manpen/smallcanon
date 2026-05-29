include(GoogleTest)

function(add_smallcanon_test name)
    add_executable(smallcanon_test_${name} ${name}.cpp)
    target_link_libraries(smallcanon_test_${name}
        PRIVATE
            smallcanon::smallcanon
            GTest::gtest_main
    )

    gtest_discover_tests(smallcanon_test_${name})
endfunction()

add_smallcanon_test(bitspan)

add_executable(smallcanon_tests
        bits.cpp
)

target_link_libraries(smallcanon_tests
    PRIVATE
        smallcanon::smallcanon
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(smallcanon_tests)

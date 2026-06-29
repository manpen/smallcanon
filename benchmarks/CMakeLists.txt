add_executable(smallcanon_benchmark_simd_sort simd_sort.cpp)
target_link_libraries(smallcanon_benchmark_simd_sort
        PRIVATE
        smallcanon::smallcanon
        benchmark::benchmark_main
)

add_executable(smallcanon_benchmark_canon canon.cpp)
target_link_libraries(smallcanon_benchmark_canon
        PRIVATE
        smallcanon::smallcanon
        benchmark::benchmark_main
)
target_compile_definitions(smallcanon_benchmark_canon
        PRIVATE
        SMALLCANON_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
)

add_executable(smallcanon_benchmark_transpose transpose.cpp)
target_link_libraries(smallcanon_benchmark_transpose
        PRIVATE
        smallcanon::smallcanon
        benchmark::benchmark_main
)

if (NAUTY)
    add_executable(smallcanon_benchmark_nauty_refine nauty_refine.cpp)
    target_link_libraries(smallcanon_benchmark_nauty_refine PRIVATE
            smallcanon::smallcanon
            nauty
            benchmark::benchmark_main
    )
    target_compile_definitions(smallcanon_benchmark_nauty_refine
            PRIVATE
            SMALLCANON_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
    )
    target_link_libraries(smallcanon_benchmark_canon PRIVATE nauty)
endif ()

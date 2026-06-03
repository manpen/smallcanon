add_executable(smallcanon_benchmark_simd_sort simd_sort.cpp)

target_link_libraries(smallcanon_benchmark_simd_sort
        PRIVATE
        smallcanon::smallcanon
        benchmark::benchmark_main
)

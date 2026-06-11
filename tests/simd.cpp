#include <gtest/gtest.h>
#include <xsimd/xsimd.hpp>

#include "smallcanon/simd/avx512defs.hpp"

namespace xs = xsimd;

template<typename T, typename R>
void assert_equal() {
    const auto test = T{}.as_batch();
    const auto ref = R{}.as_batch();
    EXPECT_TRUE(xs::all(ref == test)) << test << " " << ref;
}


TEST(SimdCompletion, AVX512) {
    using namespace smallcanon::simd::avx512defs;

    assert_equal<u64cont<0, 2>, u64xconst<0, 2, 4, 6, 8, 10, 12, 14>>();
    assert_equal<u64cont<0, 0, 1>, u64xconst<0, 0, 1, 2, 3, 4, 5, 6>>();
    assert_equal<u64cont<14, 12>, u64xconst<14, 12, 10, 8, 6, 4, 2, 0>>();
    assert_equal<u32cont<1, 2>, u32xconst<1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16>>();
    assert_equal<u16cont<2, 3>, u16xconst<2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                                          23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33>>();
}

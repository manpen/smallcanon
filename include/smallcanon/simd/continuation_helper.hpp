#pragma once

#include <xsimd/xsimd.hpp>


namespace smallcanon::simd::continuation {
    namespace details {
        template<typename T, T...>
        struct last_two;

        template<typename T, T A, T B>
        struct last_two<T, A, B> {
            static constexpr T a = A;
            static constexpr T b = B;
        };

        template<typename T, T X, T... Rest>
        struct last_two<T, X, Rest...> : last_two<T, Rest...> {};
    } // namespace details

    template<typename T, typename A, T... Vs>
    struct const_continuation {
        using lt = details::last_two<T, Vs...>;

        static constexpr T next = (lt::a < lt::b) ? (lt::b + (lt::b - lt::a)) : (lt::b - (lt::a - lt::b));

        using value_t = typename const_continuation<T, A, Vs..., next>::value_t;
    };

    template<typename T, typename A, T... Vs>
        requires(sizeof...(Vs) == xsimd::batch<T, A>::size)
    struct const_continuation<T, A, Vs...> {
        using value_t = xsimd::batch_constant<T, A, Vs...>;
    };

} // namespace smallcanon::simd::continuation

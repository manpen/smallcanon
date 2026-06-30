#pragma once

#include <bit>
#include <utility>

#include <xsimd/xsimd.hpp>

namespace smallcanon::simd {

    namespace details {
        template<size_t Offset, size_t N, typename Op, typename T, typename A>
            requires(std::has_single_bit(Offset))
        xsimd::batch<T, A> incl_prefix_step_impl(xsimd::batch<T, A> data, Op&& op) {
            if constexpr (Offset < N) {
                const auto shifted = xsimd::slide_left<Offset * sizeof(T)>(data);
                const auto updated = op(data, shifted);

                struct Generator {
                    static constexpr bool get(unsigned i, unsigned n) {
                        return i >= Offset;
                    }
                };

                constexpr auto mask = xsimd::make_batch_bool_constant<T, Generator, A>();
                return xsimd::select(mask, updated, data);
            }

            return data;
        }

        template<typename T, typename A, size_t N = xsimd::batch<T, A>::size, typename Op>
            requires(std::has_single_bit(N) && N <= xsimd::batch<T, A>::size)
        xsimd::batch<T, A> incl_prefix_impl(xsimd::batch<T, A> data, Op&& op) {

            data = incl_prefix_step_impl<1, N>(data, std::forward<Op>(op));
            data = incl_prefix_step_impl<2, N>(data, std::forward<Op>(op));
            data = incl_prefix_step_impl<4, N>(data, std::forward<Op>(op));
            data = incl_prefix_step_impl<8, N>(data, std::forward<Op>(op));
            data = incl_prefix_step_impl<16, N>(data, std::forward<Op>(op));
            data = incl_prefix_step_impl<32, N>(data, std::forward<Op>(op));
            static_assert(N <= xsimd::batch<T, A>::size);

            return data;
        }
    } // namespace details

    template<typename T, typename A, size_t N = xsimd::batch<T, A>::size>
    xsimd::batch<T, A> incl_prefix_sum(xsimd::batch<T, A> data) {
        return details::incl_prefix_impl<T, A, N>(data, [](auto a, auto b) { return a + b; });
    }

    template<typename T, typename A, size_t N = xsimd::batch<T, A>::size>
    xsimd::batch<T, A> incl_prefix_max(xsimd::batch<T, A> data) {
        return details::incl_prefix_impl<T, A, N>(data, [](auto a, auto b) { return xsimd::max(a, b); });
    }

    template<typename T, typename A, size_t N = xsimd::batch<T, A>::size>
    xsimd::batch<T, A> incl_prefix_min(xsimd::batch<T, A> data) {
        return details::incl_prefix_impl<T, A, N>(data, [](auto a, auto b) { return xsimd::min(a, b); });
    }
} // namespace smallcanon::simd

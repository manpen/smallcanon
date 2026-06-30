#pragma once

#include <algorithm>
#include <vector>

#include "smallcanon/adj_matrix.hpp"
#include "smallcanon/coloring.hpp"
#include "smallcanon/orbits.hpp"
#include "smallcanon/reorder.hpp"

namespace smallcanon {
    namespace compare {
        namespace compare_details {
            template<class SM, class SC>
            inline std::strong_ordering compare_matrix(const AdjMatrix<SM>& g0, const AdjMatrix<SC>& g1) {
                const auto n = g0.num_nodes();

                if constexpr (AdjMatrix<SM>::kMaxNodes <= 64) {
                    using word_t = AdjMatrix<SM>::word_t;
                    const auto mask = (n == std::numeric_limits<word_t>::digits) //
                                              ? ~word_t(0) //
                                              : ((word_t(1) << n) - 1);

                    for (node_t u: g0.nodes()) {
                        const auto own_row = *g0.row(u).data() & mask;
                        const auto other_row = *g1.row(u).data() & mask;

                        if (own_row != other_row) {
                            return own_row < other_row ? std::strong_ordering::less : std::strong_ordering::greater;
                        }
                    }

                    return std::strong_ordering::equal;
                }


                for (color_t u = 1; u < n; ++u) {
                    for (color_t v = 0; v < u; ++v) {
                        const bool best_edge = g0.has_edge(u, v);
                        const bool leaf_edge = g1.has_edge(u, v);
                        if (best_edge != leaf_edge) {
                            return best_edge < leaf_edge ? std::strong_ordering::less : std::strong_ordering::greater;
                        }
                    }
                }

                return std::strong_ordering::equal;
            }

            template<>
            inline std::strong_ordering compare_matrix(const AdjMatrix8& g0, const AdjMatrix8& g1) {
                const auto n = g0.num_nodes();

                const auto col_mask = (((uint64_t(1) << n) - 1) * 0x0101010101010101);
                const auto row_mask = (n == 8) ? ~uint64_t(0) : ((uint64_t(1) << (8 * n)) - 1);
                const auto mask = col_mask & row_mask;

                const auto own = read_le_u64(g0.buffer().data()) & mask;
                const auto oth = read_le_u64(g1.buffer().data()) & mask;

                return own <=> oth;
            }

            template<>
            inline std::strong_ordering compare_matrix(const AdjMatrix16& g0, const AdjMatrix16& g1) {
                using namespace smallcanon::simd::avx512defs;

                const auto n = g0.num_nodes();

                // mask out all non-existing nodes
                const auto col_mask = static_cast<uint16_t>((uint64_t(1) << n) - 1);
                const auto mask = xsimd::select(u16x16cont<0, 1>().as_batch() < n, u16x16_t::broadcast(col_mask),
                                                u16x16_t::broadcast(0));

                const auto own = matrix_to_native(g0) & mask;
                const auto oth = matrix_to_native(g1) & mask;

                const auto le = (own <= oth).mask();
                const auto ge = (own >= oth).mask();

                if (le == ge) {
                    return std::strong_ordering::equal;
                }

                const auto diff = le ^ ge;
                const auto shift =
                        __builtin_ctz(diff); // contains the lsb bit in which masks differ (it exists, as le != ne)

                return (le >> shift) & 1 ? std::strong_ordering::less : std::strong_ordering::greater;
            }
        } // namespace compare_details

        // Compare a graph colored with discrete colorings (IR tree leaves).
        // When leaves are isomorphic, orbits is updated with the resulting automorphism.
        // The <, > relations are strong, but implementation defined; in general they are NOT lexicographical,
        // and may even change between various AdjMatrix storage types.
        template<class SM, class SC>
        std::strong_ordering compare_leaves(const AdjMatrix<SM>& g0, const AdjMatrix<SC>& g1) {
            const auto n = g0.num_nodes();

            if (n != g1.num_nodes()) [[unlikely]] {
                return n < g1.num_nodes() ? std::strong_ordering::less : std::strong_ordering::greater;
            }

            return compare_details::compare_matrix(g0, g1);
        }
    } // namespace compare
} // namespace smallcanon

#pragma once

#include <algorithm>
#include <vector>

#include "smallcanon/adj_matrix.hpp"
#include "smallcanon/coloring.hpp"
#include "smallcanon/orbits.hpp"

namespace smallcanon {
    namespace compare {

        // Compare a graph colored with discrete colorings (IR tree leaves).
        // When leaves are isomorphic, orbits is updated with the resulting automorphism.
        template<class SM, class SC>
        std::strong_ordering compare(const AdjMatrix<SM>& graph, const Coloring<SC>& coloring1,
                                     const Coloring<SC>& coloring2, solver::Orbits& orbits) {
            const node_t n = graph.num_nodes();

            const auto vertex1_of_color = coloring1.compute_inverse_of_discrete(n);
            const auto vertex2_of_color = coloring2.compute_inverse_of_discrete(n);

            // Lexicographically compare the relabeled adjacency matrices
            for (color_t c_u = 0; c_u < n; ++c_u) {
                const node_t u1 = vertex1_of_color[c_u];
                const node_t u2 = vertex2_of_color[c_u];

                for (color_t c_v = 0; c_v < c_u; ++c_v) {
                    const node_t v1 = vertex1_of_color[c_v];
                    const node_t v2 = vertex2_of_color[c_v];

                    const bool e1 = graph.has_edge(u1, v1);
                    const bool e2 = graph.has_edge(u2, v2);

                    if (e1 != e2)
                        return e1 < e2 ? std::strong_ordering::less : std::strong_ordering::greater;
                }
            }

            // The two leaves are isomorphic
            for (color_t c = 0; c < n; ++c) {
                const node_t from = vertex1_of_color[c];
                const node_t to = vertex2_of_color[c];
                if (from != to)
                    orbits.union_orbits(from, to);
            }

            return std::strong_ordering::equal;
        }

    } // namespace compare
} // namespace smallcanon

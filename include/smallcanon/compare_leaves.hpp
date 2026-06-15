#pragma once

#include <algorithm>
#include <vector>

#include "smallcanon/adj_matrix.hpp"
#include "smallcanon/coloring.hpp"
#include "smallcanon/orbits.hpp"

namespace smallcanon {
    namespace compare {

        // Compare a graph colored with discrete colorings (IR tree leaves), where
        // -1 means coloring1 is smaller
        //  0 means colored graphs are isomorphic
        //  1 means coloring2 is smaller
        //  
        //  When leaves are isomorphic, orbits is updated with the resulting automorphism.
        template<class SM, class SC>
            int compare(const AdjMatrix<SM>& graph, const Coloring<SC>& coloring1, const Coloring<SC>& coloring2,
                        solver::Orbits& orbits) {
                const node_t n = graph.num_nodes();
                constexpr node_t invalid = std::numeric_limits<node_t>::max();

                // TODO should not allocate
                std::vector<node_t> vertex1_of_color(n, invalid);
                std::vector<node_t> vertex2_of_color(n, invalid);

                // Invert both discrete colorings
                for (const node_t v : graph.nodes()) {
                    const color_t c1 = coloring1.get_color(v);
                    const color_t c2 = coloring2.get_color(v);

                    assert(c1 < n);
                    assert(c2 < n);
                    assert(vertex1_of_color[c1] == invalid);
                    assert(vertex2_of_color[c2] == invalid);

                    vertex1_of_color[c1] = v;
                    vertex2_of_color[c2] = v;
                }

                // Lexicographically compare the relabeled adjacency matrices
                for (color_t c_u = 0; c_u < n; ++c_u) {
                    const node_t u1 = vertex1_of_color[c_u];
                    const node_t u2 = vertex2_of_color[c_u];

                    for (color_t c_v = 0; c_v < c_u; ++c_v) {
                        const node_t v1 = vertex1_of_color[c_v];
                        const node_t v2 = vertex2_of_color[c_v];

                        const bool e1 = graph.has_edge(u1, v1);
                        const bool e2 = graph.has_edge(u2, v2);

                        if (e1 != e2) return e1 < e2 ? -1 : 1;
                    }
                }

                // The two leaves are isomorphic
                for (color_t c = 0; c < n; ++c) {
                    const node_t from = vertex1_of_color[c];
                    const node_t to   = vertex2_of_color[c];
                    if (from != to) orbits.union_orbits(from, to);
                }

                return 0;
            }

    } // namespace selector
} // namespace smallcanon

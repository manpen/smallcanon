#pragma once

#include <algorithm>
#include <vector>

#include "smallcanon/adj_matrix.hpp"
#include "smallcanon/coloring.hpp"

namespace smallcanon {
    namespace selector {

        // find the first non-trivial color if it exists, otherwise return false
        template<class SM, class SC>
        std::optional<color_t> select_first(const AdjMatrix<SM>& graph, const Coloring<SC>& coloring) {
            const auto n = graph.num_nodes();
            std::vector<node_t> counts(n, 0); // TODO horribly inefficient

            for (node_t v = 0; v < n; ++v) {
                const auto color = coloring.get_color(v);
                assert(color >= 0);
                assert(color < n);
                ++counts[color];
            }

            for (color_t color = 0; color < n; ++color) {
                if (counts[color] >= 2) {
                    return color;
                }
            }

            return std::nullopt;
        }

    } // namespace selector
} // namespace smallcanon

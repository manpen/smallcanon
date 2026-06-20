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

            auto prev_color = coloring.color_at_label(0);
            for (node_t v = 1; v < n; ++v) {
                auto color = coloring.color_at_label(v);
                if (color == prev_color) {
                    return color;
                }
                prev_color = color;
            }

            return std::nullopt;
        }
    } // namespace selector
} // namespace smallcanon

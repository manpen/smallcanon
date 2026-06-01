#pragma once

#include <algorithm>
#include <vector>

#include "smallcanon/adj_matrix.hpp"
#include "smallcanon/coloring.hpp"

namespace smallcanon {
    namespace selector {

        // find the first non-trivial color if it exists, otherwise return false
        template<typename SM, typename SC>
        std::pair<color_t,bool> select_first(const AdjMatrix<SM>& graph, Coloring<SC>& coloring) {
            for(node_t v = 0; v < graph.num_nodes(); ++v) {
                if(coloring.get_color(v)) {
                    // TODO if non-trivial, return color
                }
            }
        }

    }
}

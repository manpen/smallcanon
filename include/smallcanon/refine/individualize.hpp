
#pragma once

#include <algorithm>
#include <vector>

#include "smallcanon/adj_matrix.hpp"
#include "smallcanon/coloring.hpp"

namespace smallcanon {
    namespace refine {
        // Very inefficient implementation meant as a reference to cross-validate more complex implementations against.
        // There may be no node i >= graph.num_nodes() with deg(i) > 0.
        template<typename SC>
        void individualize(Coloring<SC>& coloring, node_t v) {
            // TODO individualize v in coloring
        }
    } // namespace refine
} // namespace smallcanon

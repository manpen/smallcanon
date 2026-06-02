#pragma once

#include <algorithm>
#include <vector>

#include "smallcanon/adj_matrix.hpp"
#include "smallcanon/coloring.hpp"
#include "smallcanon/permutation.hpp"
#include "smallcanon/refine/individualize.hpp"
#include "smallcanon/selector.hpp"
#include "smallcanon/refine/naive.hpp"

namespace smallcanon {
    namespace solver {
        class Orbits {
            std::vector<node_t> partition;

            public:
            void clear() {
                std::ranges::iota(partition.begin(), partition.end(), 0);
            }

            Orbits(size_t num_nodes) {
                partition.resize(num_nodes);
                clear();
            }

            node_t get_representative(node_t v) {
                node_t repr = v;
                while(repr != partition[repr]) repr = partition[repr];
                partition[v] = repr;
                return repr;
            }


            bool is_representative(node_t v) {
                return v == get_representative(v);
            }

            void union_orbits(node_t v1, node_t v2) {
                const node_t repr1 = get_representative(v1);
                const node_t repr2 = get_representative(v2);
                if(repr1 < repr2) {
                    partition[repr2] = repr1;
                } else {
                    partition[repr1] = repr2;
                }
            }
        };
    }
}

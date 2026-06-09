#pragma once

#include <algorithm>
#include <cassert>
#include <vector>
#include "smallcanon/graph.hpp"


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
                assert(v < partition.size());
                node_t repr = v;
                while (repr != partition[repr])
                    repr = partition[repr];
                partition[v] = repr;
                return repr;
            }


            bool is_representative(node_t v) {
                assert(v < partition.size());
                return v == get_representative(v);
            }

            void union_orbits(node_t v1, node_t v2) {
                assert(v1 < partition.size());
                assert(v2 < partition.size());
                const node_t repr1 = get_representative(v1);
                const node_t repr2 = get_representative(v2);
                if (repr1 < repr2) {
                    partition[repr2] = repr1;
                } else {
                    partition[repr1] = repr2;
                }
            }
        };
    } // namespace solver
} // namespace smallcanon

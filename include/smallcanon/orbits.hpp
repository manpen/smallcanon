#pragma once

#include <algorithm>
#include <cassert>
#include <vector>
#include "smallcanon/graph.hpp"


namespace smallcanon {
    namespace solver {

        typedef double group_order_t;

        class Orbits {
            std::vector<node_t> partition;
            std::vector<size_t> partition_sz;

        public:
            void clear() {
                std::ranges::iota(partition.begin(), partition.end(), 0);
                std::ranges::fill(partition_sz.begin(), partition_sz.end(), 1);
            }

            Orbits(size_t num_nodes) {
                partition.resize(num_nodes);
                partition_sz.resize(num_nodes);
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

            size_t orbit_size(node_t v) {
                assert(v < partition.size());
                return partition_sz[get_representative(v)];
            }

            void union_orbits(node_t v1, node_t v2) {
                assert(v1 < partition.size());
                assert(v2 < partition.size());
                const node_t repr1 = get_representative(v1);
                const node_t repr2 = get_representative(v2);
                if (repr1 < repr2) {
                    partition[repr2] = repr1;
                    partition_sz[repr1] += partition_sz[repr2];
                } else if (repr2 > repr1) {
                    partition[repr1] = repr2;
                    partition_sz[repr2] += partition_sz[repr1];
                }
            }
        };
    } // namespace solver
} // namespace smallcanon

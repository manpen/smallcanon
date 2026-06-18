#pragma once

#include <algorithm>
#include <cassert>
#include <vector>
#include "smallcanon/graph.hpp"


namespace smallcanon {
    namespace solver {

        using group_order_t = double;

        class Orbits {
            std::vector<node_t> partition;
            std::vector<size_t> partition_sz;

        public:
            void clear() {
                std::ranges::iota(partition.begin(), partition.end(), 0);
                std::ranges::fill(partition_sz.begin(), partition_sz.end(), 1);
            }

            constexpr Orbits(size_t num_nodes) : partition(num_nodes, 0), partition_sz(num_nodes, 0) {
                clear();
            }

            [[nodiscard]] constexpr node_t get_representative(node_t v) noexcept {
                assert(v < partition.size());
                node_t repr = v;
                while (repr != partition[repr])
                    repr = partition[repr];
                partition[v] = repr;
                return repr;
            }


            [[nodiscard]] constexpr bool is_representative(node_t v) noexcept {
                assert(v < partition.size());
                return v == get_representative(v);
            }

            [[nodiscard]] constexpr size_t orbit_size(node_t v) noexcept {
                assert(v < partition.size());
                return partition_sz[get_representative(v)];
            }

            constexpr void union_orbits(const node_t v1, const node_t v2) noexcept {
                assert(v1 < partition.size());
                assert(v2 < partition.size());
                const node_t repr1 = get_representative(v1);
                const node_t repr2 = get_representative(v2);
                if (partition_sz[repr1] < partition_sz[repr2]) {
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

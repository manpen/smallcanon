#pragma once

#include <algorithm>
#include <vector>

#include "smallcanon/adj_matrix.hpp"
#include "smallcanon/coloring.hpp"

namespace smallcanon::refine {

    template<typename Graph>
    class Naive {
    public:
        using graph_t = Graph;
        using coloring_t = MatchedColoring<graph_t>::coloring_t;

    private:
        const graph_t& graph;
        std::vector<std::vector<color_t>> fingerprints;


    public:
        explicit Naive(const graph_t& g) : graph(g), fingerprints(graph.num_nodes()) {
            for (auto& f: fingerprints) {
                f.reserve(graph.num_nodes() + 2);
            }
        }

        void refine_starting_at(coloring_t& coloring, [[maybe_unused]] node_t start) {
            refine(coloring);
        }

        // Very inefficient implementation meant as a reference to cross-validate more complex implementations against.
        // There may be no node i >= graph.num_nodes() with deg(i) > 0.
        void refine(coloring_t& coloring) {
            assert(graph.num_nodes() == coloring.num_nodes());


            color_t color = 0;
            for (node_t round = 1; round < graph.num_nodes(); ++round) {
                // build fingerprints
                for (node_t u: graph.nodes()) {
                    auto& fp = fingerprints[u];
                    fp.clear();
                    fp.push_back(coloring.get_color(u));

                    for (auto v: graph.neighbors_of(u)) {
                        assert(v < graph.num_nodes());
                        fp.emplace_back(coloring.get_color(v));
                    }
                    std::sort(fp.begin() + 1, fp.end());

                    fp.push_back(static_cast<color_t>(u));
                }

                // std implements a lex comparison of vectors; shorter vectors are considered smaller -- hence,
                // smaller degree nodes get smaller colors (if not originally distinguished by color).
                std::sort(fingerprints.begin(), fingerprints.end(), [](auto& a, auto& b) {
                    return std::lexicographical_compare(a.begin(), a.end() - 1, b.begin(), b.end() - 1);
                });

                bool change = false;
                color = 0;
                for (node_t i: graph.nodes()) {
                    const auto node = fingerprints[i].back();
                    fingerprints[i].pop_back();

                    // we popped the node id from the back of the vector, so comparing the fingerprints is fine
                    color += (i > 0) && fingerprints[i] != fingerprints[i - 1];

                    const auto prev_color = coloring.get_color(node);
                    coloring.set_color(node, color);
                    change |= (color != prev_color);
                }

                if (!change || color + 1 == graph.num_nodes()) {
                    break;
                }
            }
        }
    };
} // namespace smallcanon::refine

#pragma once

#include <vector>

#include "smallcanon/adj_matrix.hpp"
#include "smallcanon/coloring.hpp"
#include "smallcanon/compare_leaves.hpp"
#include "smallcanon/graph.hpp"
#include "smallcanon/invariant.hpp"
#include "smallcanon/orbits.hpp"
#include "smallcanon/permutation.hpp"
#include "smallcanon/refine/individualize.hpp"
#include "smallcanon/refine/naive.hpp"
#include "smallcanon/selector.hpp"
#include "smallcanon/utility.hpp"

namespace smallcanon {
    namespace solver {

        template<typename SC>
        struct Leaf {
            bool has = false;
            Coloring<SC> leaf;
            Orbits orbits;
            group_order_t group_size = 1;

            explicit Leaf(node_t n) : leaf(n), orbits(n) {}

            void replace(Coloring<SC>& new_leaf) {
                leaf = new_leaf.copy();
                group_size = 1;
                orbits.clear();
                has = true;
            }

            void invalidate() {
                group_size = 1;
                orbits.clear();
                has = false;
            }
        };

        struct Stats {
            size_t ir_nodes_visited = 0;
            size_t best_leaf_update = 0;
            size_t automorphisms = 0;
            group_order_t group_size = 1;

            void print() {
                std::clog << "c " << console_bright_blue << "------------------------------------------------"
                          << console_neutral << std::endl;
                std::clog << "c " << "ir_nodes_visited=" << ir_nodes_visited << std::endl;
                std::clog << "c " << "best_leaf_update=" << best_leaf_update << std::endl;
                std::clog << "c " << "automorphisms=" << automorphisms << std::endl;
                std::clog << "c " << "group_size=" << group_size << std::endl;
                std::clog << "c " << console_bright_blue << "------------------------------------------------"
                          << console_neutral << std::endl;
            }
        };

        template<typename SC>
        struct SearchStack {
            std::vector<Coloring<SC>> base_to_coloring;
            std::vector<node_t> base_to_vertex;
            std::vector<color_t> base_to_col;
            std::vector<inv_t> base_to_inv;

            void push(Coloring<SC>& coloring, color_t selector_col, node_t vertex) {
                base_to_coloring.push_back(coloring.copy());
                base_to_vertex.push_back(vertex);
                base_to_col.push_back(selector_col);
            }

            node_t& top_vertex() {
                return base_to_vertex.back();
            }

            color_t& top_color() {
                return base_to_col.back();
            }

            Coloring<SC>& top_coloring() {
                return base_to_coloring.back();
            }

            void pop() {
                base_to_coloring.pop_back();
                base_to_vertex.pop_back();
                base_to_col.pop_back();
            }

            size_t size() {
                return base_to_coloring.size();
            }

            bool empty() {
                return base_to_coloring.empty();
            }

            void pop_to_level(size_t level) {
                while (base_to_coloring.size() > level)
                    pop();
            }
        };

        template<typename SM, typename SC>
        Coloring<SC> canonize(const AdjMatrix<SM>& graph, Coloring<SC>& coloring, Stats& stats) {
            // TODO add "configuration/result" information

            // initial color refinement
            refine::naive::refine(graph, coloring);
            int n = graph.num_nodes();
            coloring.print(n);

            // best leaf found so far
            Leaf<SC> best_leaf(n);
            std::vector<node_t> best_leaf_path;
            // length of prefix in common with best-leaf path
            size_t best_leaf_lca = 0;

            // TODO invariant
            // std::vector<inv_t> base_to_best_leaf_inv;

            // depth-first search state
            SearchStack<SC> stack;

            // additional comparison leaves and orbit partitions
            // constexpr size_t NUM_COMP_LEAFS = 1;
            // bool has_comp_leaf[NUM_COMP_LEAFS];
            // // Note: I hate this
            // std::array<Coloring<SC>, NUM_COMP_LEAFS> comp_leaf = {
            //     Coloring<SC>(n)
            // };

            // TODO save last place in-common with "best-leaf-path"
            // TODO so that we can jump there immediately when leaf matches best-leaf

            bool is_backtrack = false;
            while (true) {
                DEBUG_STREAM << "c [stack] at depth " << stack.size() << " on " << std::endl;
                coloring.print(n);
                // if we're not coming from a backtrack, select new color and put it on stack
                if (!is_backtrack) {
                    auto selector_result = selector::select_first(graph, coloring);
                    const bool discrete = !selector_result.has_value();
                    if (!discrete) {
                        DEBUG_STREAM << "c [select] selected " << selector_result.value() << std::endl;
                        coloring.print(n, selector_result.value());
                    } else
                        DEBUG_STREAM << "c [select] discrete" << std::endl;

                    if (discrete) {
                        bool this_is_the_best_leaf = false;
                        if (!best_leaf.has) { // TODO OR we're updating the leaf
                            // TODO set best_leaf_lca to parent!
                            // TODO invalidate all comp leafs?
                            // our best leaf orbits have become invalid
                            DEBUG_STREAM << "c [compare] this is the best-leaf is now" << std::endl;
                            this_is_the_best_leaf = true;
                            best_leaf.replace(coloring);
                            best_leaf_lca = stack.size();
                            best_leaf_path = stack.base_to_vertex;
                        }


                        size_t backtrack_to = stack.size();
                        if (!this_is_the_best_leaf) {
                            // (1) TODO compare invariants
                            // (2) when invariants equal, actually compare leafs
                            int compare = compare::compare(graph, best_leaf.leaf, coloring, best_leaf.orbits);
                            DEBUG_STREAM << "c [compare] result=" << compare << std::endl;

                            switch (compare) {
                                case 0:
                                    // TODO leaf agrees with best-leaf? jump to best-leaf LCA
                                    // TODO jumping to an LCA must purge all "deeper" leafs
                                    ++stats.automorphisms;
                                    backtrack_to = best_leaf_lca;
                                    DEBUG_STREAM << "c [compare] backtrack_to=" << backtrack_to << std::endl;
                                    break;
                                case 1:
                                    DEBUG_STREAM << "c [compare] this is the best-leaf is now" << std::endl;
                                    ;
                                    this_is_the_best_leaf = true;
                                    ++stats.best_leaf_update;
                                    best_leaf.replace(coloring);
                                    best_leaf_lca = stack.size();
                                    best_leaf_path = stack.base_to_vertex;
                                    break;
                                default:
                                    // this leaf is worse.
                                    break;
                            }

                            // TODO if this doesn't agree with any already-stored leaf, then...
                            // for (size_t i = 0; i < NUM_COMP_LEAFS; ++i) {
                            //     if (!has_comp_leaf[i]) {
                            //         // TODO
                            //         has_comp_leaf[i] = true;
                            //         break;
                            //     }
                            // }
                        }

                        stack.pop_to_level(backtrack_to);
                        if (stack.empty())
                            break;
                        coloring = stack.top_coloring().copy();
                        is_backtrack = true; // moving backward
                        continue;
                    } else {
                        const color_t col = selector_result.value();
                        DEBUG_STREAM << "c [stack] push vertex_id=" << 0 << ", col=" << col << std::endl;
                        stack.push(coloring, col, 0);
                    }
                }

                // check whether we're at a node on the best-leaf path
                const bool on_best_leaf_path = best_leaf.has && stack.size() == best_leaf_lca;

                // continue iterating vertices of present color on stack
                while (stack.top_vertex() < graph.num_nodes() &&
                       (coloring.get_color(stack.top_vertex()) != stack.top_color() ||
                        (on_best_leaf_path && !best_leaf.orbits.is_representative(stack.top_vertex())))) {
                    ++stack.top_vertex();
                }

                // no more vertex left to individualize? we need to backtrack
                if (stack.top_vertex() == graph.num_nodes()) {
                    DEBUG_STREAM << "c [stack] pop col=" << stack.top_color() << std::endl;

                    // if we're backtracking from the best-leaf LCA, decrement prefix length
                    // note that in all other cases, this prefix length won't change, as we're never revisiting
                    // the best leaf path downwards
                    if (on_best_leaf_path) {
                        --best_leaf_lca;
                        best_leaf.group_size *= best_leaf.orbits.orbit_size(best_leaf_path[best_leaf_lca] - 1);
                    }
                    stack.pop();

                    if (stack.empty())
                        break;
                    coloring = stack.top_coloring().copy();
                    // moving backwards
                    is_backtrack = true;
                    continue;
                }

                assert(coloring.get_color(stack.top_vertex()) == stack.top_color());
                // time to actually do some work now
                coloring = stack.top_coloring().copy();
                DEBUG_STREAM << "c [individualize] individualize vertex=" << stack.top_vertex() << std::endl;
                refine::individualize(coloring, stack.top_vertex());
                coloring.print(n);
                DEBUG_STREAM << "c [refine]" << std::endl;
                refine::naive::refine(graph, coloring); // TODO needs to take as argument a vertex
                coloring.print(n);
                ++stack.top_vertex();
                is_backtrack = false; // we're moving forward in the tree
                ++stats.ir_nodes_visited;

                // TODO compare invariant
            }

            stats.group_size = best_leaf.group_size;
            return best_leaf.leaf.copy();
        }
    } // namespace solver
} // namespace smallcanon

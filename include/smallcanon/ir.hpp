#pragma once

#include <vector>

#include "smallcanon/adj_matrix.hpp"
#include "smallcanon/coloring.hpp"
#include "smallcanon/compare_leaves.hpp"
#include "smallcanon/graph.hpp"
#include "smallcanon/invariant.hpp"
#include "smallcanon/orbits.hpp"
#include "smallcanon/permutation.hpp"
#include "smallcanon/refine/avx512intrin.hpp"
#include "smallcanon/refine/naive.hpp"
#include "smallcanon/selector.hpp"
#include "smallcanon/utility.hpp"

namespace smallcanon {
    namespace solver {

        template<typename SM, typename SC>
        struct Leaf {
            bool has_value = false; // whether there's actually a leaf stored in this struct
            Coloring<SC> leaf; // discrete coloring at leaf
            AdjMatrix<SM> graph; // graph in node label order
            std::vector<node_t> path; // individualized vertices on the root-to-leaf path

            Orbits orbits; // orbit partition related with this leaf
            group_order_t group_size = 1; // group size related with this leaf

            explicit Leaf(node_t n) : leaf(n), graph(n), orbits(n) {}

            void replace(const AdjMatrix<SM>& new_graph, const Coloring<SC>& new_leaf) {
                graph = new_graph.copy();
                leaf = new_leaf.copy();
                group_size = 1;
                orbits.clear();
                has_value = true;
            }

            void invalidate() {
                group_size = 1;
                orbits.clear();
                has_value = false;
            }
        };

        struct Stats {
            size_t ir_nodes_visited = 0;
            size_t best_leaf_update = 0;
            size_t automorphisms = 0;
            group_order_t group_size = 1;

            void print() const {
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
        class SearchStack {
            struct Element {
                Coloring<SC> coloring;
                node_t vertex;
                color_t col;
            };

            std::vector<Element> stack{};
            node_t capacity;

        public:
            constexpr explicit SearchStack(node_t capacity) : capacity(capacity) {
                stack.reserve(capacity);
            }


            constexpr void push(Coloring<SC>& coloring, color_t selector_col, node_t vertex) {
                assert(stack.size() < static_cast<size_t>(capacity));
                stack.emplace_back(std::move(coloring.copy()), vertex, selector_col);
            }

            [[nodiscard]] constexpr node_t& top_vertex() noexcept {
                return stack.back().vertex;
            }

            [[nodiscard]] constexpr color_t& top_color() noexcept {
                return stack.back().col;
            }

            [[nodiscard]] constexpr Coloring<SC>& top_coloring() noexcept {
                return stack.back().coloring;
            }

            void pop() noexcept {
                stack.pop_back();
            }

            [[nodiscard]] constexpr size_t size() const noexcept {
                return stack.size();
            }

            [[nodiscard]] constexpr bool empty() const noexcept {
                return stack.empty();
            }

            constexpr void pop_to_level(size_t level) noexcept {
                while (stack.size() > level) {
                    stack.pop_back();
                }
            }

            constexpr void copy_path_into(std::vector<node_t>& path) const {
                path.clear();
                path.reserve(stack.size());
                for (auto&& e: stack) {
                    path.push_back(e.vertex);
                }
            }
        };

        template<typename Graph, typename Refine = refine::Naive<Graph>>
        class Solver {
        public:
            using graph_t = Graph;
            using coloring_t = MatchedColoring<graph_t>::coloring_t;
            using leaf_t = Leaf<typename graph_t::storage_t, typename coloring_t::storage_t>;
            using stack_t = SearchStack<typename coloring_t::storage_t>;
            using refine_t = Refine;

        private:
            Stats stats{};
            graph_t graph;

        public:
            explicit Solver(const graph_t& graph) : graph(graph.copy()) {}

            coloring_t canonize(coloring_t& coloring) {
                // TODO add "configuration/result" information

                // initial color refinement
                auto refine = refine_t{graph};

                refine.refine(coloring);

                const node_t n = graph.num_nodes();
                coloring.print(n);

                // best leaf found so far
                leaf_t best_leaf(n);
                // length of prefix in common with best-leaf path
                size_t best_leaf_lca = 0;

                // TODO invariant
                // std::vector<inv_t> base_to_best_leaf_inv;

                // depth-first search state
                stack_t stack{graph.num_nodes()};

                // additional comparison leaves and orbit partitions
                // constexpr size_t NUM_COMP_LEAFS = 1;
                // bool has_comp_leaf[NUM_COMP_LEAFS];
                // std::array<Coloring<SC>, NUM_COMP_LEAFS> comp_leaf = {
                //     Coloring<SC>(n)
                // };

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
                        } else {
                            DEBUG_STREAM << "c [select] discrete" << std::endl;
                        }

                        if (discrete) {
                            size_t backtrack_to = stack.size();
                            if (!best_leaf.has_value) { // TODO OR we're updating the leaf
                                // TODO set best_leaf_lca to parent!
                                // TODO invalidate all comp leafs?
                                // our best leaf orbits have become invalid
                                DEBUG_STREAM << "c [compare] this is the best-leaf now" << std::endl;
                                const auto graph_reordered = reorder_graph(graph, coloring);
                                best_leaf.replace(graph_reordered, coloring);
                                best_leaf_lca = stack.size();
                                stack.copy_path_into(best_leaf.path);
                            } else {
                                // (1) TODO compare invariants
                                // (2) when invariants equal, actually compare leafs

                                const auto graph_reordered = reorder_graph(graph, coloring);
                                const auto compare = compare::compare_leaves(best_leaf.graph, graph_reordered);

                                if (compare == std::strong_ordering::equal) {
                                    // leaf agrees with best-leaf? jump to best-leaf LCA
                                    best_leaf.orbits.record_isomorphic_colorings(best_leaf.leaf, coloring);
                                    ++stats.automorphisms;
                                    backtrack_to = best_leaf_lca;
                                    DEBUG_STREAM << "c [compare] backtrack_to=" << backtrack_to << std::endl;

                                    // TODO jumping to an LCA must purge all "deeper" leaves, if they exist
                                } else if (compare == std::strong_ordering::greater) {
                                    DEBUG_STREAM << "c [compare] this is the best-leaf is now" << std::endl;
                                    ++stats.best_leaf_update;
                                    best_leaf.replace(graph_reordered, coloring);
                                    best_leaf_lca = stack.size();
                                    stack.copy_path_into(best_leaf.path);
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
                        }


                        const color_t col = selector_result.value();
                        DEBUG_STREAM << "c [stack] push vertex_id=" << 0 << ", col=" << col << std::endl;
                        stack.push(coloring, col, 0);
                    }

                    // check whether we're at a node on the best-leaf path
                    const bool on_best_leaf_path = best_leaf.has_value && stack.size() == best_leaf_lca;

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
                            best_leaf.group_size *= best_leaf.orbits.orbit_size(best_leaf.path[best_leaf_lca] - 1);
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
                    coloring.individualize(stack.top_vertex());
                    coloring.print(n);
                    DEBUG_STREAM << "c [refine]" << std::endl;
                    refine.refine_starting_at(coloring, stack.top_vertex());
                    coloring.print(n);
                    assert(coloring.is_consistent());
                    ++stack.top_vertex();
                    is_backtrack = false; // we're moving forward in the tree
                    ++stats.ir_nodes_visited;

                    // TODO compare invariant
                }

                stats.group_size = best_leaf.group_size;
                return best_leaf.leaf.copy();
            }

            [[nodiscard]] const Stats& get_stats() const noexcept {
                return stats;
            }
        };


    } // namespace solver
} // namespace smallcanon

#pragma once

#include <vector>

#include "smallcanon/adj_matrix.hpp"
#include "smallcanon/coloring.hpp"
#include "smallcanon/invariant.hpp"
#include "smallcanon/permutation.hpp"
#include "smallcanon/refine/individualize.hpp"
#include "smallcanon/selector.hpp"
#include "smallcanon/orbits.hpp"
#include "smallcanon/refine/naive.hpp"

namespace smallcanon {
    namespace solver {

        template<typename SM, typename SC>
        Permutation canonize(const AdjMatrix<SM>& graph, Coloring<SC>& coloring) {

            // initial color refinement
            refine::naive_scalar(graph, coloring);

            // depth-first search
            bool         has_best_leaf;
            Coloring<SC> best_leaf;

            bool         has_comp_leaf;
            Coloring<SC> comp_leaf;

            Orbits best_leaf_orbits(graph.num_nodes());
            
            // TODO: second (or third, ...) orbit partition for additional leafs
            // TODO: make number of additional leaves configurable?

            std::vector<inv_t>        base_to_best_leaf_inv;

            std::vector<Coloring<SC>> base_to_coloring;
            std::vector<node_t>       base_to_vertex;
            std::vector<color_t>      base_to_col;
            std::vector<inv_t>        base_to_inv;

            // TODO maintain the LCA's
            [[maybe_unused]] size_t best_leaf_lca = 0; // where was the last time we agreed with best-leaf root-to-leaf walk?
            [[maybe_unused]] size_t comp_leaf_lca = 0; // where was the last time we agreed with comp-leaf root-to-leaf walk?

            // TODO save last place in-common with "best-leaf-path"
            // TODO so that we can jump there immediately when leaf matches best-leaf

            bool is_backtrack = false;
            while(true) {

                // if we're not coming from a backtrack, select new color and put it on stack
                if(!is_backtrack) {
                    auto [col, discrete] = selector::select_first(graph, coloring);

                    if(discrete) {
                        if(!has_best_leaf) { // TODO OR we're updating the leaf
                            // TODO set best_leaf_lca to parent

                            // our best leaf orbits have become invalid
                            best_leaf_orbits.clear();
                            has_best_leaf = true;
                        }

                        if(!has_comp_leaf) {
                            // TODO
                            has_comp_leaf = true;
                        }

                        // TODO we found a leaf
                        // TODO compare to best leaf / comp leaf, potentially update
                        // TODO maintain orbit partition by creating automorphisms from leaf

                        // TODO leaf agrees with best-leaf? jump to best-leaf LCA
                        // TODO leaf agrees with comp-leaf? jump to comp-leaf LCA

                        if(base_to_coloring.empty()) break;
                        coloring = base_to_coloring.back();
                        is_backtrack = true; // moving backward
                        continue;
                    }

                    base_to_coloring.push_back(coloring);
                    base_to_vertex.push_back(0);
                    base_to_col.push_back(col);
                }

                // continue iterating vertices of present color on stack
                while(base_to_vertex.back() < graph.num_nodes() && 
                      coloring.get_color(base_to_vertex.back()) != base_to_col.back() &&
                      !best_leaf_orbits.is_representative(base_to_vertex.back())) {
                    ++base_to_vertex.back();
                }

                // no more vertex left to individualize? we need to backtrack
                if(base_to_vertex.back() == graph.num_nodes()) {
                    // TODO if we're backtracking from the best-leaf LCA, decrement it
                    
                    base_to_coloring.pop_back();
                    base_to_vertex.pop_back();
                    base_to_col.pop_back();

                    if(base_to_coloring.empty()) break;
                    coloring = base_to_coloring.back();
                    is_backtrack = true; // moving backward
                    continue;
                }

                // time to actually do some work now
                coloring = base_to_coloring.back();
                refine::individualize(coloring, base_to_vertex.back());
                refine::naive_scalar(graph, coloring); // TODO needs to take as argument a vertex
                ++base_to_vertex.back();
                is_backtrack = false; // we're moving forward in the tree

                // TODO compare invariant
            }
        }

        Permutation canonical_labeling;
        // TODO create canonical labeling from best leaf
        return canonical_labeling;
    }
}

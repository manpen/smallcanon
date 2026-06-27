#pragma once

#include <smallcanon/simd/avx512defs.hpp>

#if SMALLCANON_WITH_AVX512

#include <smallcanon/coloring.hpp>
#include <xsimd/xsimd.hpp>

namespace smallcanon::refine::avx512intrin {
    using namespace smallcanon::simd::avx512defs;


    template<typename Graph>
    class AVX512 {
        // public:
        // using graph_t = Graph;
        // using coloring_t = MatchedColoring<graph_t>::coloring_t;

        // private:
        // const graph_t& graph;

        // explicit AVX512(const graph_t& graph) : graph(graph) {}
        // void refine_starting_at(coloring_t& coloring, [[maybe_unused]] node_t start);
        //  void refine(coloring_t& coloring);
    };
} // namespace smallcanon::refine::avx512intrin

#include <smallcanon/refine/avx512intrin16.inc>
#include <smallcanon/refine/avx512intrin8.inc>

#endif

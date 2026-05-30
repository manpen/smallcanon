#pragma once

#include <concepts>
#include <ranges>
#include <utility>

namespace smallcanon {
    // Type for nodes and degrees
    using node_t = uint32_t;

    // Type for edge indices
    using edgeid_t = uint32_t;

    // Assert that we can at least fit all nodes into edgeid_t. Technically, we want node_t to fit twice into edgeid;
    // but that seems wasteful for small graphs
    static_assert(sizeof(edgeid_t) >= sizeof(node_t));

    // Type for actual edges (i.e. node pairs)
    using edge_t = std::pair<edgeid_t, node_t>;

    // Concept for a range of edges (i.e. range of node pairs)
    template<class R>
    concept edge_range_c = std::ranges::input_range<R> && //
                           std::constructible_from<edge_t, std::ranges::range_reference_t<R>>;


    // Type to represent nodes in algorithms; actual color stores may choose smaller types
    using color_t = node_t;
    static_assert(sizeof(color_t) >= sizeof(node_t));
} // namespace smallcanon

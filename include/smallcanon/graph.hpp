#pragma once

#include <cstdint>
#include <utility>

namespace smallcanon {
    using node_t = uint32_t;
    using edgeid_t = uint32_t;
    static_assert(sizeof(edgeid_t) >= sizeof(node_t)); // technically, we want node_t to fit twice into edgeid; but that
    // seems wasteful for small graphs

    using edge_t = std::pair<edgeid_t, node_t>;

    using color_t = node_t;
} // namespace smallcanon

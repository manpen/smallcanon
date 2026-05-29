#pragma once

#include <cassert>
#include <cstddef>
#include <generator>
#include <smallcanon/graph.hpp>
#include <string>
#include <string_view>
#include <tuple>

namespace smallcanon {
    namespace details {
        inline int graph6_value(char c) {
            return static_cast<int>(static_cast<unsigned char>(c)) - 63;
        }

        inline std::pair<node_t, std::size_t> parse_graph6_size(std::string_view text) {
            assert(!text.empty());

            if (text.front() != '~') {
                return {static_cast<node_t>(graph6_value(text.front())), 1};
            }

            assert(text.size() >= 4);
            assert(text[1] != '~');

            node_t n = 0;
            for (std::size_t i = 1; i < 4; ++i) {
                n = static_cast<node_t>((n << 6) | static_cast<node_t>(graph6_value(text[i])));
            }
            return {n, 4};
        }

        inline std::generator<edge_t> parse_graph6_edges(std::string text, node_t n) {
            // this is a quadratic time parser; we could go down to nearly linear time in by using BitSpan;
            // but since the graphs are always very small, I opted for the easiest version
            std::size_t char_index = 0;
            int remaining_bits = 0;
            int value = 0;

            for (node_t v = 1; v < n; ++v) {
                for (node_t u = 0; u < v; ++u) {
                    if (remaining_bits == 0) {
                        assert(char_index < text.size());
                        value = graph6_value(text[char_index++]);
                        remaining_bits = 6;
                    }

                    --remaining_bits;
                    if (((value >> remaining_bits) & 1) != 0) {
                        co_yield {u, v};
                    }
                }
            }
        }
    } // namespace details

    inline std::tuple<node_t, std::generator<edge_t>, std::string_view> parse_graph6(std::string_view text) {
        assert(!text.empty());

        constexpr size_t BITS_PER_CHAR = 6;

        const auto [n, edge_offset] = details::parse_graph6_size(text);
        text.remove_prefix(edge_offset);

        const auto edge_chars = (static_cast<std::size_t>(n) * (n - 1) / 2 + BITS_PER_CHAR - 1) / BITS_PER_CHAR;

        assert(text.size() >= edge_chars);
        auto remainder = text;
        remainder.remove_prefix(edge_chars);

        return {n, details::parse_graph6_edges(std::string{text}, n), remainder};
    }
} // namespace smallcanon

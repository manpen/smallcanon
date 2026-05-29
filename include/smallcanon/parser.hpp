#pragma once

#include <cassert>
#include <cstddef>
#include <generator>
#include <istream>
#include <optional>
#include <smallcanon/graph.hpp>
#include <string>
#include <string_view>
#include <tuple>

#include "adj_matrix.hpp"

namespace smallcanon {
    using parsed_graph6_t = std::tuple<node_t, std::generator<edge_t>, std::string_view>;

    namespace details {
        inline std::optional<int> graph6_value(char c) {
            // each character of a graph6 string contains 6 bits; they are encoded between ascii values 63 .. 127
            const auto value = static_cast<int>(static_cast<unsigned char>(c)) - 63;
            if (value < 0 || value > 63) {
                return std::nullopt;
            }
            return value;
        }

        inline std::string_view trim_whitespace(std::string_view text) {
            constexpr std::string_view whitespace = " \t\n\r\f\v";

            const auto first = text.find_first_not_of(whitespace);
            if (first == std::string_view::npos) {
                return {};
            }

            const auto last = text.find_last_not_of(whitespace);
            return text.substr(first, last - first + 1);
        }

        inline std::optional<std::pair<node_t, std::size_t>> parse_graph6_size(std::string_view text) {
            if (text.empty()) {
                return std::nullopt;
            }

            if (text.front() != '~') {
                const auto n = graph6_value(text.front());
                if (!n) {
                    return std::nullopt;
                }
                return {{static_cast<node_t>(*n), 1}};
            }

            if (text.size() < 4 || text[1] == '~') {
                return std::nullopt;
            }

            node_t n = 0;
            for (std::size_t i = 1; i < 4; ++i) {
                const auto value = graph6_value(text[i]);
                if (!value) {
                    return std::nullopt;
                }
                n = (n << 6) | static_cast<node_t>(*value);
            }
            return {{n, 4}};
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
                        value = *graph6_value(text[char_index++]);
                        remaining_bits = 6;
                    }

                    --remaining_bits;
                    if (((value >> remaining_bits) & 1) != 0) {
                        co_yield {u, v};
                    }
                }
            }
        }

        template<typename M>
        M build_fixed_adjmatrix(auto&& edges) {
            M matrix;
            for (auto [u, v]: edges) {
                matrix.add_edge(u, v);
            }
            return matrix;
        }
    } // namespace details

    inline std::optional<parsed_graph6_t> parse_graph6(std::string_view text) {
        constexpr size_t BITS_PER_CHAR = 6;

        const auto parsed_size = details::parse_graph6_size(text);
        if (!parsed_size) {
            return std::nullopt;
        }

        const auto [n, edge_offset] = *parsed_size;
        text.remove_prefix(edge_offset);

        const auto edge_chars = (static_cast<std::size_t>(n) * (n - 1) / 2 + BITS_PER_CHAR - 1) / BITS_PER_CHAR;

        if (text.size() < edge_chars) {
            return std::nullopt;
        }

        const auto graph_bits = text.substr(0, edge_chars);
        for (const char c: graph_bits) {
            if (!details::graph6_value(c)) {
                return std::nullopt;
            }
        }

        auto remainder = text;
        remainder.remove_prefix(edge_chars);

        return parsed_graph6_t{n, details::parse_graph6_edges(std::string{graph_bits}, n), remainder};
    }

    inline std::generator<parsed_graph6_t> read_dataset(std::istream& input) {
        std::string line;
        while (std::getline(input, line)) {
            auto parsed = parse_graph6(line);
            if (!parsed) {
                continue;
            }

            auto [n, edges, remainder] = std::move(*parsed);
            co_yield {n, std::move(edges), details::trim_whitespace(remainder)};
        }
    }

    inline std::generator<std::pair<std::string_view, AdjMatrixVariant>> read_graph_dataset(std::istream& input) {
        for (auto [n, edge, name]: read_dataset(input)) {
            if (n <= 8) {
                co_yield {name, {details::build_fixed_adjmatrix<AdjMatrix8>(edge)}};
                continue;
            }
            if (n <= 16) {
                co_yield {name, {details::build_fixed_adjmatrix<AdjMatrix16>(edge)}};
                continue;
            }
            if (n <= 32) {
                co_yield {name, {details::build_fixed_adjmatrix<AdjMatrix32>(edge)}};
                continue;
            }
            if (n <= 64) {
                co_yield {name, {details::build_fixed_adjmatrix<AdjMatrix64>(edge)}};
                continue;
            }
            if (n <= 128) {
                co_yield {name, {details::build_fixed_adjmatrix<AdjMatrix128>(edge)}};
                continue;
            }

            details::HeapStorage hs(n);
            AdjMatrixHeap matrix(std::move(hs));
            for (auto [u, v]: edge) {
                matrix.add_edge(u, v);
            }

            co_yield {name, std::move(matrix)};
        }
    }
} // namespace smallcanon

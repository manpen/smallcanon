#pragma once

#include <iostream>
#include <optional>
#include <utility>
#include <vector>

#include <smallcanon/graph.hpp>


using DimacsParseResult = std::tuple<int, std::vector<smallcanon::edge_t>, std::vector<int>>;

bool no_extra_tokens(std::istringstream& line) {
    std::string extra;
    return !(line >> extra);
}

std::optional<DimacsParseResult> parse_dimacs(std::istream& input) {
    bool has_header = false;
    int n = 0;
    int m = 0;
    std::vector<smallcanon::edge_t> edges;
    std::vector<int> colors;
    std::vector<bool> has_color;

    std::string text;
    std::size_t line_number = 0;
    while (std::getline(input, text)) {
        ++line_number;
        std::istringstream line{text};

        char type = '\0';
        if (!(line >> type)) {
            continue;
        }

        if (type == 'c') {
            continue;
        }

        if (type == 'p') {
            std::string edge_keyword;
            if (has_header || !(line >> edge_keyword >> n >> m) || edge_keyword != "edge" || n < 0 || m < 0 ||
                !no_extra_tokens(line)) {
                std::cerr << "Invalid DIMACS header at line " << line_number << '\n';
                return std::nullopt;
            }

            has_header = true;
            edges.reserve(static_cast<std::size_t>(m));
            colors.assign(static_cast<std::size_t>(n), 0);
            has_color.assign(static_cast<std::size_t>(n), false);
            continue;
        }

        if (!has_header) {
            std::cerr << "DIMACS record before header at line " << line_number << '\n';
            return std::nullopt;
        }

        if (type == 'e') {
            int u = 0;
            int v = 0;
            if (!(line >> u >> v) || !no_extra_tokens(line) || u < 1 || u > n || v < 1 || v > n ||
                static_cast<int>(edges.size()) >= m) {
                std::cerr << "Invalid DIMACS edge at line " << line_number << '\n';
                return std::nullopt;
            }

            edges.emplace_back(static_cast<smallcanon::edgeid_t>(u - 1), static_cast<smallcanon::node_t>(v - 1));
            continue;
        }

        if (type == 'n') {
            int u = 0;
            int color = 0;
            if (!(line >> u >> color) || !no_extra_tokens(line) || u < 1 || u > n || color < 1 ||
                has_color[static_cast<std::size_t>(u - 1)]) {
                std::cerr << "Invalid DIMACS color at line " << line_number << '\n';
                return std::nullopt;
            }

            colors[static_cast<std::size_t>(u - 1)] = color - 1;
            has_color[static_cast<std::size_t>(u - 1)] = true;
            continue;
        }

        std::cerr << "Unknown DIMACS record type at line " << line_number << ": " << type << '\n';
        return std::nullopt;
    }

    if (!has_header) {
        std::cerr << "Missing DIMACS header\n";
        return std::nullopt;
    }

    if (static_cast<int>(edges.size()) != m) {
        std::cerr << "Invalid DIMACS edge count: expected " << m << ", got " << edges.size() << '\n';
        return std::nullopt;
    }

    return DimacsParseResult{n, std::move(edges), std::move(colors)};
}

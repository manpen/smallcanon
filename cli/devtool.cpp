#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <chrono>

#include "devtool_args.hpp"
#include "dimacs_parser.hpp"
#include "smallcanon/adj_matrix.hpp"
#include "smallcanon/coloring.hpp"
#include "smallcanon/ir.hpp"
#include "smallcanon/utility.hpp"

static std::optional<DimacsParseResult> read_instance(const Options& option) {
    if (!option.input) {
        auto parsed = parse_dimacs(std::cin);
        if (!parsed) {
            std::cerr << "Failed to parse DIMACS input from stdin\n";
        }
        return parsed;
    }

    std::ifstream input{*option.input};
    if (!input) {
        std::cerr << "Unable to open input file: " << *option.input << '\n';
        return std::nullopt;
    }

    auto parsed = parse_dimacs(input);
    if (!parsed) {
        std::cerr << "Failed to parse DIMACS input: " << *option.input << '\n';
    }
    return parsed;
}

template<typename G>
int process_instance(const Options& option, DimacsParseResult& instance) {
    using adjmat_t = G;
    using coloring_t = typename smallcanon::MatchedColoring<adjmat_t>::coloring_t;

    // fetch instance
    auto& [inp_nodes, inp_edges, inp_colors] = instance;
    const auto nodes = static_cast<smallcanon::node_t>(inp_nodes);

    if (nodes > adjmat_t::MAX_NODES) {
        std::cerr << "Instance too large for selected container format. Instance size: " << inp_nodes
                  << ". Limit: " << adjmat_t::MAX_NODES << std::endl;
        return -1;
    }

    // convert instance into requested data types
    auto graph = adjmat_t(nodes);
    auto coloring = coloring_t{nodes};
    {


        for (auto [u, v]: inp_edges) {
            if (u == v) {
                std::cerr << "Found self-loop on node " << u << std::endl;
                return -1;
            }
            graph.add_edge(u, v);
        }

        for (auto u: graph.nodes()) {
            coloring.set_color(u, inp_colors[u]);
        }
    }

    smallcanon::solver::Stats stats;
    auto start = std::chrono::steady_clock::now();
    smallcanon::solver::canonize(graph, coloring, stats);
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double, std::milli>(end - start).count();

    stats.print();
    std::cout <<"c " << console_bright_blue << "solve_time=" << elapsed << "ms\n" << console_neutral;

    return 0;
}

int main(int argc, char **argv) {
    const auto opt_options = parse_options(argc, argv);
    if (!opt_options) {
        print_usage(std::cerr, argc > 0 ? argv[0] : "devtool");
        return 1;
    }
    auto option = opt_options.value();

    auto opt_instance = read_instance(option);
    if (!opt_instance) {
        return 1;
    }
    auto instance = *opt_instance;

    switch (option.nodes) {
        case NodeLimit::n8:
            return process_instance<smallcanon::AdjMatrix8>(option, instance);
        case NodeLimit::n16:
            return process_instance<smallcanon::AdjMatrix16>(option, instance);
        case NodeLimit::n32:
            return process_instance<smallcanon::AdjMatrix32>(option, instance);
        case NodeLimit::n64:
            return process_instance<smallcanon::AdjMatrix64>(option, instance);
        case NodeLimit::n128:
            return process_instance<smallcanon::AdjMatrix128>(option, instance);
        default:
            return process_instance<smallcanon::AdjMatrixHeap>(option, instance);
    }
}

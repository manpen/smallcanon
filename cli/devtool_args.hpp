#pragma once


enum class NodeLimit {
    n8 = 8,
    n16 = 16,
    n32 = 32,
    n64 = 64,
    n128 = 128,
    heap = 0,
};

struct Options {
    NodeLimit nodes = NodeLimit::heap;
    std::optional<std::filesystem::path> input;
};

void print_usage(std::ostream& out, std::string_view program) {
    out << "Usage: " << program << " [-n <8|16|32|64|128|h>] [-i <input-path>]\n";
}

std::optional<NodeLimit> parse_nodes(std::string_view value) {
    if (value == "8") {
        return NodeLimit::n8;
    }
    if (value == "16") {
        return NodeLimit::n16;
    }
    if (value == "32") {
        return NodeLimit::n32;
    }
    if (value == "64") {
        return NodeLimit::n64;
    }
    if (value == "128") {
        return NodeLimit::n128;
    }
    if (value == "h" || value == "H") {
        return NodeLimit::heap;
    }
    return std::nullopt;
}

std::optional<std::string_view> consume_value(int& index, int argc, char **argv, std::string_view option) {
    if (index + 1 >= argc) {
        std::cerr << "Missing value for " << option << '\n';
        return std::nullopt;
    }

    ++index;
    return std::string_view{argv[index]};
}

std::optional<Options> parse_options(int argc, char **argv) {
    Options options;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};

        if (arg == "-h" || arg == "--help") {
            print_usage(std::cout, argv[0]);
            return std::nullopt;
        }

        if (arg == "-n" || arg == "--nodes") {
            const auto value = consume_value(i, argc, argv, arg);
            if (!value) {
                return std::nullopt;
            }
            const auto nodes = parse_nodes(*value);
            if (!nodes) {
                std::cerr << "Invalid value for " << arg << ": " << *value << '\n';
                return std::nullopt;
            }
            options.nodes = *nodes;
            continue;
        }

        if (arg.starts_with("--nodes=")) {
            const auto value = arg.substr(std::string_view{"--nodes="}.size());
            const auto nodes = parse_nodes(value);
            if (!nodes) {
                std::cerr << "Invalid value for --nodes: " << value << '\n';
                return std::nullopt;
            }
            options.nodes = *nodes;
            continue;
        }

        if (arg == "-i" || arg == "--input") {
            const auto value = consume_value(i, argc, argv, arg);
            if (!value) {
                return std::nullopt;
            }
            options.input = std::filesystem::path{*value};
            continue;
        }

        if (arg.starts_with("--input=")) {
            options.input = std::filesystem::path{arg.substr(std::string_view{"--input="}.size())};
            continue;
        }

        std::cerr << "Unknown argument: " << arg << '\n';
        return std::nullopt;
    }

    return options;
}

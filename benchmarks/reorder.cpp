#include <smallcanon/bitspan.hpp>
#include <smallcanon/reorder.hpp>

#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

namespace {
    template<typename Graph, smallcanon::node_t NumNodes>
    struct BenchmarkCase {
        using graph_t = Graph;
        static constexpr smallcanon::node_t kNumNodes = NumNodes;
    };

    template<typename Graph>
    void set_matrix_bit(Graph& graph, smallcanon::node_t u, smallcanon::node_t v) {
        smallcanon::BitSpan(graph.row(u)).set_bit(v);
    }

    template<typename Graph>
    Graph make_random_graph(smallcanon::node_t num_nodes, uint32_t seed) {
        Graph graph(num_nodes);
        std::mt19937 rng(seed);
        std::bernoulli_distribution is_set(0.35);

        for (const auto u: graph.nodes()) {
            for (const auto v: graph.nodes()) {
                if (is_set(rng)) {
                    set_matrix_bit(graph, u, v);
                }
            }
        }

        return graph;
    }

    template<typename Graph>
    std::vector<smallcanon::node_t> make_random_labels(const Graph& graph, uint32_t seed) {
        std::vector<smallcanon::node_t> labels(graph.num_nodes());
        std::ranges::copy(graph.nodes(), labels.begin());

        std::mt19937 rng(seed);
        std::ranges::shuffle(labels, rng);

        return labels;
    }

    template<typename Graph>
    auto make_coloring_with_label_order(const Graph& graph, const std::vector<smallcanon::node_t>& labels) {
        typename smallcanon::MatchedColoring<Graph>::coloring_t coloring(graph.num_nodes());
        auto coloring_labels = coloring.labels();
        auto colors = coloring.colors();

        for (smallcanon::node_t pos = 0; pos < graph.num_nodes(); ++pos) {
            const auto label = labels[pos];
            coloring_labels[pos] = static_cast<typename decltype(coloring)::scolor_t>(label);
            colors[label] = static_cast<typename decltype(coloring)::scolor_t>(pos);
        }

        return coloring;
    }

    template<typename Case>
    void bm_transpose(benchmark::State& state) {
        using Graph = typename Case::graph_t;
        static auto graph = make_random_graph<Graph>(Case::kNumNodes, 0x5A17C0DE);

        for (auto _: state) {
            benchmark::DoNotOptimize(graph.buffer().data());
            auto transposed = smallcanon::transpose(graph);
            benchmark::DoNotOptimize(transposed);
            benchmark::ClobberMemory();
        }

        const auto bits = static_cast<int64_t>(Case::kNumNodes) * static_cast<int64_t>(Case::kNumNodes);
        state.SetItemsProcessed(state.iterations() * bits);
    }

    template<typename Case>
    void bm_reorder_graph(benchmark::State& state) {
        using Graph = typename Case::graph_t;
        static auto graph = make_random_graph<Graph>(Case::kNumNodes, 0x7E57C0DE);
        static const auto labels = make_random_labels(graph, 0x9E3779B9);
        static auto coloring = make_coloring_with_label_order(graph, labels);

        for (auto _: state) {
            benchmark::DoNotOptimize(graph.buffer().data());
            benchmark::DoNotOptimize(coloring.labels().data());
            auto reordered = smallcanon::reorder_graph(graph, coloring);
            benchmark::DoNotOptimize(reordered);
            benchmark::ClobberMemory();
        }

        const auto bits = static_cast<int64_t>(Case::kNumNodes) * static_cast<int64_t>(Case::kNumNodes);
        state.SetItemsProcessed(state.iterations() * bits);
    }

    using AdjMatrix8Case = BenchmarkCase<smallcanon::AdjMatrix8, 8>;
    using AdjMatrix16Case = BenchmarkCase<smallcanon::AdjMatrix16, 16>;
    using AdjMatrix32Case = BenchmarkCase<smallcanon::AdjMatrix32, 32>;
    using AdjMatrix64Case = BenchmarkCase<smallcanon::AdjMatrix64, 64>;
    using AdjMatrixHeapCase = BenchmarkCase<smallcanon::AdjMatrixHeap, 65>;

    BENCHMARK_TEMPLATE(bm_transpose, AdjMatrix8Case);
    BENCHMARK_TEMPLATE(bm_transpose, AdjMatrix16Case);
    BENCHMARK_TEMPLATE(bm_transpose, AdjMatrix32Case);
    BENCHMARK_TEMPLATE(bm_transpose, AdjMatrix64Case);
    BENCHMARK_TEMPLATE(bm_transpose, AdjMatrixHeapCase);

    BENCHMARK_TEMPLATE(bm_reorder_graph, AdjMatrix8Case);
    BENCHMARK_TEMPLATE(bm_reorder_graph, AdjMatrix16Case);
    BENCHMARK_TEMPLATE(bm_reorder_graph, AdjMatrix32Case);
    BENCHMARK_TEMPLATE(bm_reorder_graph, AdjMatrix64Case);
    BENCHMARK_TEMPLATE(bm_reorder_graph, AdjMatrixHeapCase);
} // namespace

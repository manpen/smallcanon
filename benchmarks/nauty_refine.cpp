#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

#include <benchmark/benchmark.h>

#include <smallcanon/adj_matrix.hpp>
#include <smallcanon/parser.hpp>

#include <smallcanon/coloring.hpp>
#include <smallcanon/refine/avx512intrin.hpp>
#include <smallcanon/refine/naive.hpp>

extern "C" {
#include "nauty.h"
}

namespace {
    template<size_t MaxN>
    struct NautyInstances {
        static constexpr size_t kMaxN = MaxN;
        static constexpr size_t kMaxM = SETWORDSNEEDED(kMaxN);

        int n;
        int m;
        std::array<graph, kMaxN * kMaxM> g{};
        std::array<int, kMaxN> lab{};
        std::array<int, kMaxN> ptn{};
        std::array<int, kMaxN> orbits{};
        std::array<int, kMaxN> scratch{};
        std::array<setword, kMaxM> active{};

        NautyInstances(int n) : n(n), m(SETWORDSNEEDED(n)) {
            assert(n <= kMaxN);
            EMPTYGRAPH(g.data(), m, n);
        }

        void add_edge(int u, int v) {
            ADDONEEDGE(g.data(), u, v, m);
        }

        void reset_aux() {
            for (int i = 0; i < n; ++i) {
                lab[i] = i;
                ptn[i] = 1;
                ADDELEMENT(active.data(), i);
            }
            ptn[n - 1] = 0;
        }
    };

    template<typename G>
    auto load_nauty_instances() {
        constexpr size_t kMaxN = G::storage_t::CAPACITY;
        using instance_t = NautyInstances<kMaxN>;
        std::vector<instance_t> instances;
        for (auto fn: {"all_n8.g6", "all_m11.g6", "curated.g6"}) {
            const auto dataset_path = std::filesystem::path(SMALLCANON_PROJECT_ROOT) / "datasets" / fn;
            std::ifstream dataset_file(dataset_path);
            for (auto [name, graph]: smallcanon::read_graph_dataset(dataset_file)) {
                std::visit(
                        [&](auto&& adjmat) {
                            using T = std::decay_t<decltype(adjmat)>;
                            if constexpr (std::is_same_v<T, G>) {
                                instances.emplace_back(static_cast<int>(adjmat.num_nodes()));

                                for (auto [u, v]: adjmat.edges()) {
                                    instances.back().add_edge(static_cast<int>(u), static_cast<int>(v));
                                }
                            }
                        },
                        graph);
            }
        }

        std::cout << "Instances (for " << kMaxN << "): " << instances.size() << std::endl;

        return instances;
    }

    template<typename G>
    void bm_nauty_refine(benchmark::State& state) {
        static auto instances = load_nauty_instances<G>();

        if (instances.empty()) {
            state.SkipWithMessage("No instances");
            return;
        }


        int numcells = 1;
        int code = 0;

        size_t idx = instances.size() - 1;

        for (auto _: state) {
            if (++idx == instances.size()) {
                state.PauseTiming();
                for (auto& inst: instances) {
                    inst.reset_aux();
                }
                state.ResumeTiming();
                idx = 0;
            }

            auto& inst = instances[idx];

            numcells = 1;
            refine(inst.g.data(), inst.lab.data(), inst.ptn.data(), 0, &numcells, inst.scratch.data(),
                   inst.active.data(), &code, inst.m, inst.n);
            benchmark::DoNotOptimize(inst);
        }
    }

    template<typename G>
    struct SmallCanonInstance {
        using graph_t = G;
        using coloring_t = typename smallcanon::MatchedColoring<G>::coloring_t;

        graph_t graph;
        coloring_t coloring;

        explicit SmallCanonInstance(graph_t&& graph) : graph{std::move(graph)}, coloring(graph.num_nodes()) {}

        void reset_aux() {
            for (auto node: graph.nodes()) {
                coloring.set_color(node, 0);
            }
        }
    };

    template<typename G>
    auto load_smallcanon_instances() {
        std::vector<SmallCanonInstance<G>> instances;
        for (auto fn: {"all_n8.g6", "all_m11.g6", "curated.g6"}) {
            const auto dataset_path = std::filesystem::path(SMALLCANON_PROJECT_ROOT) / "datasets" / fn;
            std::ifstream dataset_file(dataset_path);
            for (auto [name, graph]: smallcanon::read_graph_dataset(dataset_file)) {
                std::visit(
                        [&](auto&& adjmat) {
                            using T = std::decay_t<decltype(adjmat)>;
                            if constexpr (std::is_same_v<T, G>) {
                                instances.emplace_back(std::forward<T>(adjmat));
                            }
                        },
                        graph);
            }
        }

        return instances;
    }


    template<typename G>
    void bm_naive(benchmark::State& state) {
        static auto instances = load_smallcanon_instances<G>();

        if (instances.empty()) {
            state.SkipWithMessage("No instances");
            return;
        }

        size_t idx = 0;
        for (auto _: state) {
            if (++idx == instances.size()) {
                state.PauseTiming();
                for (auto& inst: instances) {
                    inst.reset_aux();
                }
                state.ResumeTiming();
                idx = 0;
            }

            auto& inst = instances[idx];

            smallcanon::refine::naive::refine(inst.graph, inst.coloring);

            benchmark::DoNotOptimize(inst);
        }
    }

#if XSIMD_WITH_AVX512F
    template<typename G>
    void bm_avx512_instrinsic(benchmark::State& state) {
        static auto instances = load_smallcanon_instances<G>();

        if (instances.empty()) {
            state.SkipWithMessage("No instances");
            return;
        }

        size_t idx = 0;
        for (auto _: state) {
            if (++idx == instances.size()) {
                state.PauseTiming();
                for (auto& inst: instances) {
                    inst.reset_aux();
                }
                state.ResumeTiming();
                idx = 0;
            }

            auto& inst = instances[idx];

            smallcanon::refine::avx512intrin::refine(inst.graph, inst.coloring);

            benchmark::DoNotOptimize(inst);
        }
    }
#endif
} // namespace

#if XSIMD_WITH_AVX512F
BENCHMARK_TEMPLATE(bm_avx512_instrinsic, smallcanon::AdjMatrix8);
#endif
BENCHMARK_TEMPLATE(bm_nauty_refine, smallcanon::AdjMatrix8);
BENCHMARK_TEMPLATE(bm_naive, smallcanon::AdjMatrix8);

BENCHMARK_TEMPLATE(bm_nauty_refine, smallcanon::AdjMatrix16);
BENCHMARK_TEMPLATE(bm_nauty_refine, smallcanon::AdjMatrix32);

// BENCHMARK_TEMPLATE(bm_naive, smallcanon::AdjMatrix16);
// BENCHMARK_TEMPLATE(bm_naive, smallcanon::AdjMatrix32);

#include <benchmark/benchmark.h>
#include "instances.hpp"

#include <smallcanon/ir.hpp>

template<typename G>
void bm_canon(benchmark::State& state) {
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

        auto solver = smallcanon::solver::Solver(inst.graph);
        const auto res = solver.canonize(inst.coloring);

        benchmark::DoNotOptimize(res);
    }
}

template<typename G>
void bm_canon_avx512(benchmark::State& state) {
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

        auto solver = smallcanon::solver::Solver<G, smallcanon::refine::avx512intrin::AVX512<G>>(inst.graph);
        const auto res = solver.canonize(inst.coloring);

        benchmark::DoNotOptimize(res);
    }
}

BENCHMARK_TEMPLATE(bm_canon, smallcanon::AdjMatrix8);
BENCHMARK_TEMPLATE(bm_canon_avx512, smallcanon::AdjMatrix8);
BENCHMARK_TEMPLATE(bm_canon, smallcanon::AdjMatrix16);

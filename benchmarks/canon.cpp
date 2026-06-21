#include <array>

#include <benchmark/benchmark.h>
#include "instances.hpp"

#include <smallcanon/ir.hpp>

#if SMALLCANON_WITH_NAUTY
extern "C" {
#include "nauty.h"
}

template<typename G>
void bm_canon_nauty(benchmark::State& state) {
    static auto instances = load_nauty_instances<G>();

    if (instances.empty()) {
        state.SkipWithMessage("No instances");
        return;
    }


    using instance_t = std::decay_t<decltype(instances.front())>;

    static DEFAULTOPTIONS_GRAPH(options);
    options.getcanon = TRUE;

    statsblk stats{};
    std::array<graph, instance_t::kMaxN * instance_t::kMaxM> canong{};

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

        densenauty(inst.g.data(), inst.lab.data(), inst.ptn.data(), inst.orbits.data(), &options, &stats, inst.m,
                   inst.n, canong.data());

        benchmark::DoNotOptimize(canong);
    }
}
#endif


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
        auto res = solver.canonize(inst.coloring);

        benchmark::DoNotOptimize(res);
    }
}

#if XSIMD_WITH_AVX512F
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
        auto res = solver.canonize(inst.coloring);

        benchmark::DoNotOptimize(res);
    }
}

BENCHMARK_TEMPLATE(bm_canon_avx512, smallcanon::AdjMatrix8);
BENCHMARK_TEMPLATE(bm_canon_avx512, smallcanon::AdjMatrix16);
#endif

#if SMALLCANON_WITH_NAUTY
BENCHMARK_TEMPLATE(bm_canon_nauty, smallcanon::AdjMatrix8);
BENCHMARK_TEMPLATE(bm_canon_nauty, smallcanon::AdjMatrix16);
#endif

BENCHMARK_TEMPLATE(bm_canon, smallcanon::AdjMatrix8);
BENCHMARK_TEMPLATE(bm_canon, smallcanon::AdjMatrix16);

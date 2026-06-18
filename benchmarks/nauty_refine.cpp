#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>

#include <benchmark/benchmark.h>

#include <smallcanon/adj_matrix.hpp>
#include <smallcanon/parser.hpp>

#include <smallcanon/refine/avx512intrin.hpp>
#include <smallcanon/refine/naive.hpp>

#include "instances.hpp"

extern "C" {
#include "nauty.h"
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
        refine(inst.g.data(), inst.lab.data(), inst.ptn.data(), 0, &numcells, inst.scratch.data(), inst.active.data(),
               &code, inst.m, inst.n);
        benchmark::DoNotOptimize(inst);
    }
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

#if XSIMD_WITH_AVX512F
BENCHMARK_TEMPLATE(bm_avx512_instrinsic, smallcanon::AdjMatrix8);
#endif
BENCHMARK_TEMPLATE(bm_nauty_refine, smallcanon::AdjMatrix8);
BENCHMARK_TEMPLATE(bm_naive, smallcanon::AdjMatrix8);

BENCHMARK_TEMPLATE(bm_nauty_refine, smallcanon::AdjMatrix16);
BENCHMARK_TEMPLATE(bm_nauty_refine, smallcanon::AdjMatrix32);

// BENCHMARK_TEMPLATE(bm_naive, smallcanon::AdjMatrix16);
// BENCHMARK_TEMPLATE(bm_naive, smallcanon::AdjMatrix32);

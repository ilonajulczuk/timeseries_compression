#include <vector>
#include <utility>

#include "benchmark/benchmark.h"
#include "compression.h"

static void BM_AddTSPoints(benchmark::State& state) {
    compression::Encoder encoder{};

    for (auto _ : state) {
        for (int i = 0; i < state.range(0); i++) {
            encoder.Append(i * 10, i + 0.3);
        }
    }
}
// Register the function as a benchmark, with powers of 8 as arguments.
BENCHMARK(BM_AddTSPoints)->Range(8, 8<<8);;


static void BM_AddTSPointsAndDecode(benchmark::State& state) {
    for (auto _ : state) {
        compression::Encoder encoder{};
        std::vector<std::pair<int, double>> res;
        for (unsigned int i = 0; i < state.range(0); i++) {
            encoder.Append(i * 10, i + 0.3);
        }
        for (auto pair : encoder) {
            res.push_back(pair);
        }
        benchmark::DoNotOptimize(res.size());
    }
}

// Register the function as a benchmark with powers of 8 as arguments.
BENCHMARK(BM_AddTSPointsAndDecode)->Range(8, 8<<8);;

BENCHMARK_MAIN();
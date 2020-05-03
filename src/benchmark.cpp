//
// Created by nerull on 01.05.2020.
//
#include <benchmark/benchmark.h>
#include <fmt/os.h>
#include "vertx/ClusteredMessage.h"

class Fixture : public benchmark::Fixture {
public:
    void SetUp(const benchmark::State &state) override {

    }

    void TearDown(const benchmark::State &state) override {
    }


};

BENCHMARK_DEFINE_F(Fixture, format_int) (benchmark::State& state) {
    for (auto _ : state) {
        std::string addr = std::string("127.0.0.1") + ":" + fmt::format_int(33333).str();
    }
}

BENCHMARK_REGISTER_F(Fixture, format_int)->Unit(benchmark::kNanosecond);

BENCHMARK_DEFINE_F(Fixture, format_int_cstr) (benchmark::State& state) {
    for (auto _ : state) {
        std::string addr = std::string("127.0.0.1") + ":" + fmt::format_int(33333).c_str();
    }
}

BENCHMARK_REGISTER_F(Fixture, format_int_cstr)->Unit(benchmark::kNanosecond);

BENCHMARK_DEFINE_F(Fixture, to_string) (benchmark::State& state) {
    for (auto _ : state) {
        std::string addr = std::string("127.0.0.1") + ":" + std::to_string(33333);
    }
}

BENCHMARK_REGISTER_F(Fixture, to_string)->Unit(benchmark::kNanosecond);

BENCHMARK_MAIN()ClusteredMessage;
#include <iostream>
#include <chrono>
#include <tuple>
#include <thread>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <functional>

namespace chrono = std::chrono;

// disabled because it is too slow compared to mt19937.h
#ifdef USE_STD_RANDOM
#pragma message "Using std random"
#include <random>
thread_local std::mt19937 rnd_engine;
#elif defined(USE_SFMT)
#pragma message "Using SFMT"
#include "sfmt/SFMT.h"
thread_local sfmt_t rnd_engine;
#else
#pragma message "Using mt19937.h"
#include "mt19937.h" // modified to be thread safe
#endif

template <size_t a, size_t b>
struct unif_generator {
#ifdef USE_STD_RANDOM
    typedef std::uniform_real_distribution<double> distribution_type;
    distribution_type dist = distribution_type(a, std::nextafter(b, std::numeric_limits<double>::max())); // so it includes b
#else
    size_t range = b - a;
#endif

    double operator()() {
#ifdef USE_STD_RANDOM
        return dist(rnd_engine);
#elif defined(USE_SFMT)
        return a + sfmt_genrand_real1(&rnd_engine) * range;
#else
        return a + (genrand() * range);
#endif
    }
};

struct acumulator {
    double simple = 0.0;
    double squared = 0.0;
};

auto T1 = unif_generator<40, 56>{};
auto T2 = unif_generator<24, 32>{};
auto T3 = unif_generator<20, 40>{};
auto T4 = unif_generator<16, 48>{};
auto T5 = unif_generator<10, 30>{};
auto T6 = unif_generator<15, 30>{};
auto T7 = unif_generator<20, 25>{};
auto T8 = unif_generator<30, 50>{};
auto T9 = unif_generator<40, 60>{};
auto T10 = unif_generator<8, 16>{};

static void estimate_range(std::vector<acumulator> &mem, size_t i, size_t begin_index, size_t end_index) {
    acumulator acc; // faster than updating mem[i] on each iteration
    for (size_t j = begin_index; j < end_index; ++j) {
        auto x1 = T1();
        auto x2 = T2();
        auto x3 = T3();
        auto x4 = T4();
        auto x5 = T5();
        auto x6 = T6();
        auto x7 = T7();
        auto x8 = T8();
        auto x9 = T9();
        auto x10 = T10();

        auto t2f = x1 + x2;
        auto t3f = x1 + x3;
        auto t4f = std::max(t2f, t3f) + x4;
        auto t5f = std::max(t2f, t3f) + x5;
        auto t6f = t3f + x6;
        auto t7f = t3f + x7;
        auto t8f = std::max({t4f, t5f, t6f, t7f}) + x8;
        auto t9f = t5f + x9;
        auto t10f = std::max({t7f, t8f, t9f}) + x10;

        acc.simple += t10f;
        acc.squared += (t10f*t10f);
    }
    mem[i] = acc;
}

static chrono::microseconds run_simulation(size_t N, size_t num_threads) {
    typedef chrono::duration<long double, std::milli> float_milliseconds;

    chrono::steady_clock::time_point begin = chrono::steady_clock::now();

    std::vector<std::thread> threads;
    std::vector<acumulator> partial_results(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        // this will construct the thread IN the vector
        threads.emplace_back([&, i] {
#ifdef USE_STD_RANDOM
            rnd_engine.seed((i+1)*10000);
#elif defined(USE_SFMT)
            sfmt_init_gen_rand(&rnd_engine, (i+1)*10000);
#else
            sgenrand((i+1)*10000); // different seed for each thread
#endif
            estimate_range(partial_results, i, i * N / num_threads, (i + 1) * N / num_threads);
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    double x_hat = std::accumulate(partial_results.begin(),
                                   partial_results.end(),
                                   0.0,
                                   std::bind(std::plus<double>(),
                                             std::placeholders::_1,
                                             std::bind(&acumulator::simple,
                                                       std::placeholders::_2)));
    x_hat = x_hat / N;

    double v_hat = std::accumulate(partial_results.begin(),
                                   partial_results.end(),
                                   0.0,
                                   std::bind(std::plus<double>(),
                                             std::placeholders::_1,
                                             std::bind(&acumulator::squared,
                                                       std::placeholders::_2)));
    v_hat = v_hat / (N * (N - 1)) - (x_hat * x_hat) / (N -1);

    auto duration = chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - begin);

    std::cout << "samples: " << N << " (10^" << std::log10(N) << ")" << std::endl;
    std::cout << "x_hat:   " << x_hat << std::endl;
    std::cout << "v_hat:   " << v_hat << std::endl;
    std::cout << "stddev:  " << std::sqrt(v_hat) << " (as sqrt of v_hat)" << std::endl;
    std::cout << "time:    " << chrono::duration_cast<float_milliseconds>(duration).count() << " ms" << std::endl;

    return duration;
}

int main() {
    auto max_duration = chrono::seconds(60);
    auto hwc = std::thread::hardware_concurrency();
    std::cout << hwc << " concurrent threads are supported." << std::endl;
    std::cout << "--------------------------" << std::endl;

    chrono::steady_clock::time_point begin = chrono::steady_clock::now();

    size_t N = 1;
    while (true) {
        N *= 10;
        auto dur = run_simulation(N, hwc);
        std::cout << "--------------------------" << std::endl;
        if (dur > max_duration) {
            break;
        }
    }

    auto duration = chrono::steady_clock::now() - begin;
    std::cout << "total duration: " << chrono::duration_cast<chrono::milliseconds>(duration).count() << " ms" << std::endl;
}

#include <iostream>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <tuple>
#include <thread>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <functional>
#include <array>
#include <shared_mutex>
#include "sfmt/SFMT.h"


#include <libQRNG.h>

namespace chrono = std::chrono;

typedef std::array<double, 6> M_Vector;

static const M_Vector hs_center{ {0.45, 0.5, 0.6, 0.6, 0.5, 0.45} };
static const double hs_radius = 0.35;
static const double hs_sqradius = hs_radius * hs_radius;

static std::vector<double> qrng_state;
//static std::atomic_size_t qrng_table_ptr{0};
static size_t qrng_table_ptr = 0;
static double qrng_rand_from_table() {
    return qrng_state[qrng_table_ptr++];
}


struct QRNG {
    double nums[10000];
    int max = 0;
    int ptr = 0;

    QRNG(const char *user, const char *password) {
        int res = qrng_connect(user, password);
        if (res != QRNG_SUCCESS) {
            throw std::runtime_error(qrng_error_strings[res]);
        }
    }

    double operator()() {
        while (ptr == max) {
            int res = qrng_get_double_array(nums, sizeof(nums)/sizeof(nums[0]), &max);
            if (res != QRNG_SUCCESS) {
                throw std::runtime_error(qrng_error_strings[res]);
            }
            ptr = 0;
        }
        return nums[ptr++];
    }
};

static __thread QRNG gen;

static void run_simulation(const size_t N,
    const size_t num_threads,
    const bool extra_restrictions) {
    typedef chrono::duration<long double, std::milli> float_milliseconds;

    std::vector<std::thread> threads;
    std::vector<double> partial_results(num_threads);

    chrono::steady_clock::time_point begin_tp = chrono::steady_clock::now();

    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i] {
            sfmt_t rnd_state;
            sfmt_init_gen_rand(&rnd_state, (i + 1) * 10000);
            auto acc = 0lu;
            for (auto beg = i * N / num_threads, end = (i + 1) * N / num_threads;
                beg < end;
                ++beg) {
#ifdef USE_TABLE
                M_Vector point{ {qrng_rand_from_table(),
                                 qrng_rand_from_table(),
                                 qrng_rand_from_table(),
                                 qrng_rand_from_table(),
                                 qrng_rand_from_table(),
                                 qrng_rand_from_table()} };

#else
                M_Vector point{ {sfmt_genrand_real1(&rnd_state),
                                 sfmt_genrand_real1(&rnd_state),
                                 sfmt_genrand_real1(&rnd_state),
                                 sfmt_genrand_real1(&rnd_state),
                                 sfmt_genrand_real1(&rnd_state),
                                 sfmt_genrand_real1(&rnd_state)} };
#endif
                M_Vector distance_vector{ {point[0] - hs_center[0],
                                          point[1] - hs_center[1],
                                          point[2] - hs_center[2],
                                          point[3] - hs_center[3],
                                          point[4] - hs_center[4],
                                          point[5] - hs_center[5]} };
                auto sq_distance = std::inner_product(distance_vector.begin(), distance_vector.end(), distance_vector.begin(), 0.0);

                if (sq_distance <= hs_sqradius &&
                    (!extra_restrictions || (3 * point[0] + 7 * point[3] <= 5 &&
                        point[2] + point[3] <= 1 &&
                        point[0] - point[1] - point[4] + point[5] >= 0))) {
                    acc += 1;
                }
            }
            partial_results[i] = acc;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto lambda_hat = std::accumulate(partial_results.begin(), partial_results.end(), 0.0);
    lambda_hat = lambda_hat / N;

    auto variance = lambda_hat * (1 - lambda_hat) / (N - 1);

    auto duration = chrono::duration_cast<float_milliseconds>(chrono::steady_clock::now() - begin_tp);

    std::cout << "samples:   " << N << " (10^" << std::log10(N) << ")" << std::endl;
    std::cout << "λ(R):      " << std::scientific << std::setprecision(5) << lambda_hat << std::endl;
    std::cout << "Var[λ(R)]: " << std::scientific << std::setprecision(5) << variance << std::endl;
    std::cout << "stddev:    " << std::scientific << std::setprecision(5) << std::sqrt(variance) << std::endl;
    std::cout << "time:      " << std::fixed << std::setprecision(3) << duration.count() << " ms" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <N> <random_numbers_file>" << std::endl;
        return 1;
    }

    size_t N;
    try {
        N = std::stoll(argv[1]);
    }
    catch (std::invalid_argument& e) {
        std::cerr << "Invalid argument: " << argv[1] << std::endl;
        return 1;
    }

    std::ifstream rnd_file(argv[2]);
    if (!rnd_file.is_open()) {
        std::cerr << "Could not open file: " << argv[2] << std::endl;
        return 1;
    }

    qrng_state.reserve(10000000);

    while (rnd_file.good()) {
        double rnd;
        rnd_file >> rnd;
        qrng_state.push_back(rnd);
    }

    auto hw = std::thread::hardware_concurrency();
#ifdef USE_TABLE
    hw = 1;
#endif
    run_simulation(N, hw, true);
    return 0;
}

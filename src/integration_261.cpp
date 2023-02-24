#include <iostream>
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
#include "sfmt/SFMT.h"

#include <boost/math/distributions/normal.hpp>

namespace chrono = std::chrono;
namespace math = boost::math;

typedef std::array<double, 2> Vector2;

static const Vector2 center{ {0.5, 0.5} };
static const double radius = 0.4;
static const double sqradius = radius * radius;
static const double height = 8.0;

static double K_fn(const Vector2& point) {
    double dist_sq = std::pow(point[0] - center[0], 2) +
                     std::pow(point[1] - center[1], 2);

    // valid only if it is inside the circle
    if (dist_sq <= sqradius) {
        // apply the height function
        return height - height / radius * sqrt(dist_sq);
    }

    // outside the circle: return 0
    return 0;
}

static double run_simulation(const size_t N) {
    typedef chrono::duration<long double, std::milli> float_milliseconds;

    sfmt_t rnd_state;
    sfmt_init_gen_rand(&rnd_state, 35141);

    auto S = 0.0;
    auto T = 0.0;

    auto begin_tp = chrono::steady_clock::now();

    // initial asignment to prevent conditional
    // jumps in the for loop
    Vector2 point{{sfmt_genrand_real1(&rnd_state),
                    sfmt_genrand_real1(&rnd_state)}};
    S = K_fn(point);

    for (size_t j = 1; j < N; ++j) {
        // sort a point in the (x,y) plane
        Vector2 point{{sfmt_genrand_real1(&rnd_state),
                       sfmt_genrand_real1(&rnd_state)}};
        auto k_of_z = K_fn(point);
        // work with j+1 to cope with the initial assignment
        // note: uses previous value of S so it cannot be run
        // in parallel
        T += (1.0 - 1.0 / (j+1)) * std::pow(k_of_z - S/j, 2);
        S += k_of_z;
    }

    auto duration = chrono::duration_cast<float_milliseconds>(chrono::steady_clock::now() - begin_tp);

    auto z_hat = S / N;
    auto sigma_sq = T / (N - 1);
    auto var_of_z = sigma_sq / N;

    const auto delta = 0.05;
    math::normal ndist;
    auto error = math::quantile(ndist, 1 - delta/2) * std::pow(sigma_sq/N, 0.5);

    std::cout << "samples: " << N << " (10^" << std::log10(N) << ")" << std::endl;
    std::cout << "ζ̈(R)   : " << std::scientific << std::setprecision(5) << z_hat << std::endl;
    std::cout << "Var(K) : " << std::scientific << std::setprecision(5) << sigma_sq << std::endl;
    std::cout << "Var(ζ̈) : " << std::scientific << std::setprecision(5) << var_of_z << std::endl;
    std::cout << "Error  : " << std::scientific << std::setprecision(5) << error << std::endl;
    std::cout << "Time   : " << std::fixed << std::setprecision(3) << duration.count() << " ms" << std::endl;

    return sigma_sq;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <N> <threads>" << std::endl;
        return 1;
    }

    size_t N;
    try {
        N = std::stoll(argv[1]);
    } catch (std::invalid_argument& e) {
        std::cerr << "Invalid argument: " << argv[1] << std::endl;
        return 1;
    }

    // cannot run in multiple threads
    auto sigma_sq = run_simulation(N);

    // parte B
    auto DELTA = 0.05;
    auto EPSILON = 0.001;
    math::normal normdist{};
    size_t nN = std::ceil(std::pow(math::quantile(normdist, 1 - DELTA/2), 2) * sigma_sq / std::pow(EPSILON, 2));

    std::cout << "-----------------" << std::endl;
    std::cout << "nN = " << nN << std::endl;
    run_simulation(nN);

    //nN = std::ceil(stats.norm.ppf(1 - DELTA/2)**2 * sigma_sq / EPSILON**2)
    //nN

    return 0;
}

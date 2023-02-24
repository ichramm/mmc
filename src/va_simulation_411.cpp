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
#include <random>
#include "sfmt/SFMT.h"

#include <boost/math/distributions/normal.hpp>

namespace chrono = std::chrono;
namespace math = boost::math;

typedef std::array<double, 2> Vector2;

static const Vector2 center{ {0.5, 0.5} };
static const double radius = 0.4;
static const double sqradius = radius * radius;
static const double height = 8.0;

#ifdef USE_CDFLIB
// slower than boost and std::normal_distribution
extern double dinvnr ( double *p, double *q );
#endif

#ifndef USE_INVERSE_TRANSFORM
#define USE_INVERSE_TRANSFORM 1
#endif

static inline double rational_approximation(double p) {
    // Abramowitz and Stegun formula 26.2.23. (|error| < 4.5e-4)
    static double c[] = {2.515517, 0.802853, 0.010328};
    static double d[] = {1.432788, 0.189269, 0.001308};
    return p - ((c[2]*p + c[1])*p + c[0]) / (((d[2]*p + d[1])*p + d[0])*p + 1.0);
}

static inline double normal_cdf_inverse(double p) {
    if (p < 0.5) { // F^-1(p) = - G^-1(p)
        return -rational_approximation(std::sqrt(-2.0 * std::log(p)));
    } else { // F^-1(p) = G^-1(1-p)
        return rational_approximation(std::sqrt(-2.0 * std::log(1-p)));
    }
}

// random number with normal distribution using inverse transform by default
static double random_normal(sfmt_t &rnd_state) {
#if USE_INVERSE_TRANSFORM
    auto x = sfmt_genrand_real1(&rnd_state);
#ifdef USE_CDFLIB
    auto q = 1 - x;
    return dinvnr(&x, &q);
#else
    //static math::normal dist;
    //return quantile(dist, x);
    // A-S formula is 60% faster than boost with no apparent loss of precision
    return normal_cdf_inverse(x);
#endif // USE_CDFLIB

#else // !USE_INVERSE_TRANSFORM
    static std::default_random_engine generator;
    static std::normal_distribution<double> distribution(0, 1);
    return distribution(generator);
#endif
}

// random number with x^2 distribution using inverse transform
static inline double random_squared(sfmt_t &rnd_state) {
    auto x = sfmt_genrand_real1(&rnd_state);
    return std::sqrt(x);
}

static inline double K_fn(const Vector2& point) {
    double dist_sq = std::pow(point[0] - center[0], 2) +
                     std::pow(point[1] - center[1], 2);

    // valid only if it is inside the circle
    if (dist_sq <= sqradius) {
        // apply the height function
        return height - height / radius * std::sqrt(dist_sq);
    }

    // outside the circle: return 0
    return 0;
}

static inline Vector2 toss_point(sfmt_t &rnd_state) {
    auto r = random_squared(rnd_state);
    auto z1 = random_normal(rnd_state);
    auto z2 = random_normal(rnd_state);
    auto norm = std::sqrt(z1*z1 + z2*z2);
    auto x1 = r*z1 / norm;
    auto x2 = r*z2 / norm;
    // transform x1 and x2 to fit the circle of center (0.5, 0.5) and radius 0.4
    x1 = 0.5 + x1*radius;
    x2 = 0.5 + x2*radius;
    return Vector2{{x1, x2}};
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
    auto point = toss_point(rnd_state);
    S = K_fn(point);

    math::normal ndist;
    for (size_t j = 1; j < N; ++j) {
        point = toss_point(rnd_state);
        auto k_of_z = K_fn(point);
        // work with j+1 to cope with the initial assignment
        // note: uses previous value of S so it cannot be run
        // in parallel
        T += (1.0 - 1.0 / (j+1)) * std::pow(k_of_z - S/j, 2);
        S += k_of_z;
    }

#if 0
El cálculo de la varianza es incorrecto (y por esto no se encuentran valores distintos a los anteriores).
Específicamente:
 - es necesario, o multiplicar k_z por el área del círculo *antes* de sumar a S y a T;
 - o, como se hizo en el código, normalizar S (multiplicado por el área), y (faltante en
   el código) normalizar T por el cuadrado del área.
Lo primero (multiplicar k por el área) es lo más sencillo y acorde a lo discutido en el
curso, si bien la otra alternativa es igualmente válida.
#endif

    // normalize S by multiplying by the area of the circle
    S *= (radius * radius * M_PI);

    // normalize T by multiplying by square of the area of the circle
    T *= (radius * radius * M_PI);

    auto duration = chrono::duration_cast<float_milliseconds>(chrono::steady_clock::now() - begin_tp);

    auto z_hat = S / N;
    auto sigma_sq = T / (N - 1);
    auto var_of_z = sigma_sq / N;

    const auto delta = 0.05;
    auto error = math::quantile(ndist, 1 - delta/2) * std::pow(sigma_sq/N, 0.5);

    std::cout << "samples    : " << N << " (10^" << std::log10(N) << ")" << std::endl;
    std::cout << "ζ̈(R)       : " << std::scientific << std::setprecision(5) << z_hat << std::endl;
    std::cout << "Var(K)     : " << std::scientific << std::setprecision(5) << sigma_sq << std::endl;
    std::cout << "Var(ζ̈)     : " << std::scientific << std::setprecision(5) << var_of_z << std::endl;
    std::cout << "Error (95%): " << std::scientific << std::setprecision(5) << error << std::endl;
    std::cout << "Time       : " << std::fixed << std::setprecision(3) << duration.count() << " ms" << std::endl;

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

    auto sigma_sq = run_simulation(N);

    // parte B
    auto DELTA = 0.05;
    auto EPSILON = 0.001;
    math::normal normdist{};
    size_t nN = std::ceil(std::pow(math::quantile(normdist, 1 - DELTA/2), 2) * sigma_sq / std::pow(EPSILON, 2));

    std::cout << "-----------------" << std::endl;
    std::cout << "nN = " << nN << std::endl;
    run_simulation(nN);

    return 0;
}

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

typedef chrono::duration<long double, std::milli> float_milliseconds;

/*
 * X = (a_1, ... a_m): Student i gets assigned to professor a_i (with m = num_students)
 * |X| = num_professors^num_students
 * k = 1 and 2
 * S_1 = match language
 * S_2 = student count per professor
 */

size_t constexpr power(size_t base, size_t exponent) {
    return exponent == 0 ? 1 : base * power(base, exponent - 1);
}

template<size_t NumStudents>
struct Set_Function {
    typedef std::function<bool(const std::array<size_t, NumStudents>&)> type;
};

template <size_t NumStudents, size_t NumProfessors>
std::array<size_t, NumStudents> random_assignment(sfmt_t& rnd_state) {
    std::array<size_t, NumStudents> assignment;
    for (size_t i = 0; i < NumStudents; ++i) {
        assignment[i] = sfmt_genrand_uint32(&rnd_state) % NumProfessors;
    }
    // P(assignment) = 1 / |X| = 1 / num_professors^num_students
    return assignment;
}

template <size_t r,
          size_t K,
          typename SolutionMaker,
          typename SetFunction>
auto montecarlo_couting(size_t N,
                        SolutionMaker solution_maker,
                        std::array<SetFunction, K> subsets,
                        double delta) {
    size_t S = 0;

    for (size_t i = 0; i < N; ++i) {
        auto solution = solution_maker();
        if (std::find_if_not(subsets.begin(),
                             subsets.end(),
                             [&solution](const SetFunction& subsets) {
                                 return subsets(solution);
                             }) == subsets.end()) {
            ++S;
        }
    }

    auto Cn = r * S / N;
    auto VCn = Cn * (r - Cn) / (N - 1);
    auto StdDev = std::sqrt(VCn);

    math::normal ndist;
    double n_hat = N + 4;
    double p_hat = (S + 2) / n_hat;
    auto errorAC = r * math::quantile(ndist, 1 - delta/2) * (1 / std::sqrt(n_hat)) * std::sqrt(p_hat * (1 - p_hat));
    auto errorN = math::quantile(ndist, 1 - delta/2) * std::sqrt(VCn);

    return std::make_tuple(Cn, VCn, StdDev, errorAC, errorN);
}

template <size_t NumStudents, size_t NumProfesors, size_t K>
auto students_assignment(size_t N,
                         const std::array<typename Set_Function<NumStudents>::type, K>& set_functions,
                         double delta = 0.05) {
    sfmt_t rnd_state;
    sfmt_init_gen_rand(&rnd_state, 54321);

    // cardinality of the solution space
    constexpr size_t r = power(NumProfesors, NumStudents);

    return montecarlo_couting<r, K>(N,
                                    [&rnd_state]() {
                                        return random_assignment<NumStudents, NumProfesors>(rnd_state);
                                    },
                                    set_functions,
                                    delta);
}


int main(int argc, char * argv[]) {
    auto N = 1000;

    if (argc > 1) {
        N = std::stoll(argv[1]);
    }

    // bit flags
    const uint8_t SPANISH    = 1;
    const uint8_t ENGLISH    = 2;
    const uint8_t FRENCH     = 4;
    const uint8_t PORTUGUESE = 8;

    std::array<size_t, 10> students = {{SPANISH | ENGLISH,          // Maria
                                        ENGLISH | FRENCH,           // Sophie
                                        SPANISH | PORTUGUESE,       // Liliana
                                        ENGLISH | PORTUGUESE,       // Lucia
                                        FRENCH,                     // Monique
                                        SPANISH | ENGLISH | FRENCH, // Rodrigo
                                        ENGLISH,                    // John
                                        PORTUGUESE | SPANISH,       // Neymar
                                        FRENCH | PORTUGUESE,        // Jacques
                                        SPANISH} };                 // Juan

    std::array<size_t, 4> professors = {{ENGLISH | FRENCH | SPANISH, // Tom
                                         ENGLISH | PORTUGUESE,       // Luciana
                                         ENGLISH | FRENCH,           // Gerard
                                         SPANISH | FRENCH} };        // Silvia

    // solution is a NumStudents vector of integers in [0, NumProfessors)
    typedef std::array<size_t, students.size()> Solution_Type;

    // predicate that tells whether a solution is in the set S1, with S1 being
    // the set of assignments where the student shares at least one language with the professor
    auto language_matches = [&](const Solution_Type& assignment) {
        for (size_t student = 0; student < assignment.size(); ++student) {
            auto professor = assignment[student];

            if (0 == (professors[professor] & students[student])) {
                return false;
            }
        }

        return true;
    };

    {
        std::cout << "\nCounting with only one restriction" << std::endl;
        auto begin_tp = chrono::steady_clock::now();
        auto [Cn, VCn, StdDev, errorAC, errorN] = students_assignment<students.size(), professors.size(),  1>(N, {language_matches});
        auto duration = chrono::duration_cast<float_milliseconds>(chrono::steady_clock::now() - begin_tp);
        std::cout << "samples : " << N << " (10^" << (int)std::log10(N) << ")" << std::endl;
        std::cout << "time    : " << std::fixed << std::setprecision(3) << duration.count() << " ms" << std::endl;
        std::cout << "Cn      : " << (size_t)Cn << std::endl;
        std::cout << "VCn     : " << (size_t)VCn << std::endl;
        std::cout << "StdDev  : " << (size_t)StdDev << std::endl;
        std::cout << "Error AC: " << (size_t)errorAC << std::endl;
        std::cout << "Error N : " << (size_t)errorN << std::endl;
    }

    // predicate that tells whether a solution is in the set S2, with S2 being the set
    // of assignments where each processor has at least one student and no more than 4
    auto student_count_check = [&](const Solution_Type& assignment) {
        std::array<size_t, professors.size()> counts = {0};
        for (auto professor : assignment) {
            ++counts[professor];
        }
        return std::find_if(counts.begin(), counts.end(),
                            [](size_t count) {
                                return count == 0 || count > 4;
                            }) == counts.end();
    };

    {
        std::cout << "\nCounting with 2 restrictions" << std::endl;
        auto begin_tp = chrono::steady_clock::now();
        auto [Cn, VCn, StdDev, errorAC, errorN] = students_assignment<students.size(), professors.size(), 2>(N, {language_matches, student_count_check});
        auto duration = chrono::duration_cast<float_milliseconds>(chrono::steady_clock::now() - begin_tp);
        std::cout << "samples : " << N << " (10^" << (int)std::log10(N) << ")" << std::endl;
        std::cout << "time    : " << std::fixed << std::setprecision(3) << duration.count() << " ms" << std::endl;
        std::cout << "Cn      : " << (size_t)Cn << std::endl;
        std::cout << "VCn     : " << (size_t)VCn << std::endl;
        std::cout << "StdDev  : " << (size_t)StdDev << std::endl;
        std::cout << "Error AC: " << (size_t)errorAC << std::endl;
        std::cout << "Error N : " << (size_t)errorN << std::endl;
    }

    return 0;
}

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

namespace chrono = std::chrono;

int main() {
  const size_t total_runs = 1000000;
  typedef chrono::duration<long double, std::milli> float_milliseconds;

  float_milliseconds rand_dur, mt_dur;
  sfmt_t rnd_state;
  srand(1234);
  sfmt_init_gen_rand(&rnd_state, 1234);

  for (int i = 0; i < 10; ++i) {

    chrono::steady_clock::time_point begin_tp = chrono::steady_clock::now();
    for (size_t j = 0; j < total_runs; ++j) {
      rand();
    }
    rand_dur += (chrono::steady_clock::now() - begin_tp);

    begin_tp = chrono::steady_clock::now();
    for (size_t j = 0; j < total_runs; ++j) {
      sfmt_genrand_real1(&rnd_state);
    }
    mt_dur += (chrono::steady_clock::now() - begin_tp);
  }

  std::cout << "rand: " << std::fixed << std::setprecision(4) << rand_dur.count() << " ms" << std::endl;
  std::cout << "mt  : " << std::fixed << std::setprecision(4) << mt_dur.count() << " ms" << std::endl;

  return 0;
}

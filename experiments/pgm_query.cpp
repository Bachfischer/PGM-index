//
// Created by Giorgio Vinciguerra on 27/04/2020.
// Modified by Ryan Marcus, 29/04/2020.
//

/*
 * Compile current with:
 *   g++ poisoning.cpp -std=c++17 -I../include -o poisoning
 * Run with:
 *   ./poisoning
 */

/*
 * Compile old version with:
 *   g++ poisoning.cpp -std=c++17 -I../include -Xpreprocessor -fopenmp -lomp -o poisoning
 * Run with:
 *   ./poisoning
 */

#include <fstream>
#include <iostream>
#include <random>
#include <chrono>
#include "pgm/pgm_index.hpp"

#define BRANCHLESS

// #define OLD_VERSION

using timer = std::chrono::high_resolution_clock;

uint64_t NUM_LOOKUPS = 10000000;

std::vector<std::string> DATASET_NAMES = {
  "data/books_200M_uint64"
};

// Function taken from https://github.com/gvinciguerra/rmi_pgm/blob/357acf668c22f927660d6ed11a15408f722ea348/main.cpp#L29.
// Authored by Giorgio Vinciguerra.
template<class ForwardIt, class T, class Compare = std::less<T>>
ForwardIt lower_bound_branchless(ForwardIt first, ForwardIt last, const T &value, Compare comp = Compare()) {
    auto n = std::distance(first, last);

    while (n > 1) {
        auto half = n / 2;
        __builtin_prefetch(&*first + half / 2, 0, 0);
        __builtin_prefetch(&*first + half + half / 2, 0, 0);
        first = comp(*std::next(first, half), value) ? first + half : first;
        n -= half;
    }

    return std::next(first, comp(*first, value));
}

template<class T>
void do_not_optimize(T const &value) {
    asm volatile("" : : "r,m"(value) : "memory");
}

template<typename T>
static std::vector<T> load_data(const std::string &filename) {
    std::vector<T> data;

    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "unable to open " << filename << std::endl;
        exit(EXIT_FAILURE);
    }

    // Read size.
    uint64_t size;
    in.read(reinterpret_cast<char *>(&size), sizeof(uint64_t));
    data.resize(size);

    // Read values.
    in.read(reinterpret_cast<char *>(data.data()), size * sizeof(T));
    in.close();

    return data;
}

// Use the below code to generate lookup keys drawn from the data keys. This ensures
// that every part of the dataset is accessed equally.

std::vector<std::pair<uint64_t, size_t>> generate_queries(std::vector<uint64_t>& dataset) {
  std::vector<std::pair<uint64_t, size_t>> results;
  results.reserve(NUM_LOOKUPS);

  std::mt19937 g(42);
  std::uniform_int_distribution<size_t> distribution(0, dataset.size());
  
  for (uint64_t i = 0; i < NUM_LOOKUPS; i++) {
    size_t idx = distribution(g);
    uint64_t key = dataset[idx];
    size_t correct_lb = std::distance(
      dataset.begin(),
      std::lower_bound(dataset.begin(), dataset.end(), key)
      );
    
    results.push_back(std::make_pair(key, correct_lb));
  }

  return results;
  }

// Use the below code to generate lookup keys drawn uniformly from the minimum
// and maximum data key. This can lead to misleading results when the underlying dataset
// has skew:
//   consider a dataset where most values range between 0 and 2^50, but the last
//   20 keys range between 2^51 and 2^64. Over 99% of uniformly drawn lookups will
//   only access the last 20 keys.
//
// When this is the case, all you are really testing is your CPU cache. The FB dataset
// demonstrates this.

/*
std::vector<std::pair<uint64_t, size_t>> generate_queries(std::vector<uint64_t>& dataset) {
  std::vector<std::pair<uint64_t, size_t>> results;
  results.reserve(NUM_LOOKUPS);
  
  std::mt19937 g(42);
  std::uniform_int_distribution<uint64_t> distribution(dataset.front(), dataset.back() - 1);
  
  for (uint64_t i = 0; i < NUM_LOOKUPS; i++) {
    uint64_t key = distribution(g);
    size_t correct_lb = std::distance(dataset.begin(), std::lower_bound(dataset.begin(), dataset.end(), key));
    
    results.push_back(std::make_pair(key, correct_lb));
  }
  
  return results;
}
*/

template<typename F, class V>
size_t query_time(F f, const V &queries) {
  auto start = timer::now();
  
  uint64_t cnt = 0;
  for (auto &q : queries) {
    cnt += f(q.first, q.second);
  }
  do_not_optimize(cnt);
  
    auto stop = timer::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count() / queries.size();
}


void measure_perfomance() {

  int pred_correct = 0;
  int pred_incorrect = 0;

  std::string dataset_name = "data/books_200M_uint64";
  //std::cout << "Reading " << dataset_name << std::endl;
  auto dataset = load_data<uint64_t>(dataset_name);

  //std::cout << "Generating queries..." << std::endl;
  std::vector<std::pair<uint64_t, size_t>> queries = generate_queries(dataset);

  // Test lookups for PGM.
  
  // Construct the PGM-index
  //const int epsilon = 128; // space-time trade-off parameter

  const int epsilon = 128; // space-time trade-off parameter
  #ifdef OLD_VERSION
    PGMIndex<uint64_t, epsilon> index(dataset);
  #else
    pgm::PGMIndex<uint64_t, epsilon> index(dataset);
  #endif

  auto pgm_ns = query_time([&index, &dataset, &pred_correct, &pred_incorrect](auto x, auto correct_idx) {
    #ifdef OLD_VERSION
      auto approx_range = index.find_approximate_position(x);

    #else
      auto approx_range = index.search(x);

    #endif

#ifdef BRANCHLESS
    auto lb_result = lower_bound_branchless(dataset.begin() + approx_range.lo, dataset.begin() + approx_range.hi, x);
#else
    auto lb_result = std::lower_bound(dataset.begin() + approx_range.lo, dataset.begin() + approx_range.hi, x);
#endif

    size_t lb_position = std::distance(dataset.begin(), lb_result);

    //std::cout << "Lower bound position: " << lb_position << std::endl;
    
    if (lb_position != correct_idx) {
      std::cout << "PGM returned incorrect result for lookup key " << x << std::endl;
      std::cout << "Start: " << approx_range.lo
                << " Stop: " << approx_range.hi
                << " Correct: " << correct_idx << std::endl;
      pred_incorrect = pred_incorrect +1;
   
    } else {
      // Correct prediction
      /*
      std::cout << "PGM returned correct result for lookup key " << x << std::endl;
      std::cout << "Start: " << approx_range.lo
                << " Stop: " << approx_range.hi
                << " Correct: " << correct_idx << std::endl;
      */
     pred_correct = pred_correct + 1;
    }
    return lb_position;
  }, queries);

  std::cout << dataset_name << ":" << pgm_ns << std::endl;
  std::cout << "Correct: " << pred_correct << " - Incorrect: " << pred_incorrect << std::endl;
}

int main(int argc, char **argv) {

    std::cout << "Dataset - PGM" << std::endl;
    std::cout << "Books" << std::endl;
    measure_perfomance();
    return 0;
}

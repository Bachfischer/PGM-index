/*
 * This example shows how to use pgm::DynamicPGMIndex, a std::map-like container supporting inserts and deletes.
 * Compile with:
 *   g++ pgm_update_books.cpp -std=c++17 -I../include -o pgm_update_books
 * Run with:
 *   ./pgm_update_books
 */
#include <fstream>
#include <chrono>
#include <random>
#include <set>


#include <vector>
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include "pgm/pgm_index_dynamic.hpp"

std::vector<std::string> DATASET_NAMES = {
  "data/books_200M_uint64"
};

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

int main() {
    std::string dataset_name = "data/books_200M_uint64";
    auto dataset = load_data<uint64_t>(dataset_name);
    
    
    int nelements = 1000000;

    /*
    Generate random data from lognormal distribution
    Source: https://github.com/stanford-futuredata/index-baselines
    */

    double scale = 1e+9;
    double max = double(INT_MAX) / scale;
    
    
    std::mt19937 rng;
    rng.seed(std::random_device()());
    std::lognormal_distribution<double> dist(0.0, 2.0);


    // Generate some random key-value pairs to bulk-load the Dynamic PGM-index
    std::set<int> samples;

    while (samples.size() < nelements) {
        double r = dist(rng);
        if (r > max) continue;
        samples.insert(int(r * scale));
        if (samples.size() % nelements == 0) {
            std::cerr << "Generated " << samples.size() << std::endl;
        }
    }    

    /*
    Insert data into vector 
    */

    std::vector<int> data(samples.begin(), samples.end());
    std::sort(data.begin(), data.end());

    // Construct aempty Dynamic PGM-index
    pgm::DynamicPGMIndex<int, uint32_t> dynamic_pgm{};

    std::cout << "Inserting data" << std::endl;
    for (std::vector<int>::iterator it = data.begin() ; it != data.end(); ++it) {
        dynamic_pgm.insert_or_assign(*it, 1);

    }

    std::cout << "Number of elements in container: " << dynamic_pgm.size() << std::endl;
    std::cout << "Index size in bytes: " << dynamic_pgm.index_size_in_bytes() << std::endl;

    std::cout << "Deleting half of dataset" << std::endl;

    /*
    Erase datapoints from index 
    */
    int counter = 0;
    for (std::vector<int>::iterator it = data.begin() ; it != data.end(); ++it) {
        if (counter % 2 == 0) {
            dynamic_pgm.erase(*it);
        }
        counter++;
    } 

    int pred_correct = 0;
    int pred_incorrect = 0;
    int correct_idx = 0;

    counter = 0;

    /*
    Search every element (deleted or not) from index
    */
    for (std::vector<int>::iterator it = data.begin() ; it != data.end(); ++it) {

        //std::cout << "find(" << *it << "): " << std::endl;

        /* Measure time required to find data point /*
        using std::chrono::high_resolution_clock;
        using std::chrono::duration_cast;
        using std::chrono::duration;
        using std::chrono::nanoseconds;

        /* 
        
        auto t1 = high_resolution_clock::now();
        
        // std::cout << (dynamic_pgm.find(*it) == dynamic_pgm.end() ? "not found" : "found") << std::endl;

        if (dynamic_pgm.find(*it) == dynamic_pgm.end()) {
            // datapoint not found
            continue;
            

        } else {
            // datapoint found
            auto t2 = high_resolution_clock::now();
                    
            //Getting number of nanoseconds as an integer. 
            auto ns_int = duration_cast<nanoseconds>(t2 - t1);
            
            std::cout <<  ns_int.count() << std::endl;

        }
        */
        dynamic_pgm.find(*it);
 

        /*
        // Dynamic PGM does not support search - code used for reference

        auto approx_range = dynamic_pgm.search(*it);

        auto lb_result = std::lower_bound(data.begin() + approx_range.lo, data.begin() + approx_range.hi, *it);

        size_t lb_position = std::distance(data.begin(), lb_result);

        if (lb_position != correct_idx) {
            std::cout << "PGM returned incorrect result for lookup key " << *it << std::endl;
            std::cout << "Start: " << approx_range.lo << " Stop: " << approx_range.hi << " Correct: " << correct_idx << std::endl;
            pred_incorrect++;
        } else {
            std::cout << "PGM returned correct result for lookup key " << *it << std::endl;
            std::cout << "Start: " << approx_range.lo << " Stop: " << approx_range.hi << " Correct: " << correct_idx << std::endl;
            pred_correct++;
        }
        */
    

        // Update the correct index counter
        correct_idx++;

    } 

    //Print prediction results  
    //std::cout << "Correct: " << pred_correct << " - Incorrect: " << pred_incorrect << std::endl;
    
    /*
    Generate random samples that have not been added to the index before
    */
    std::set<int> samples_never_seen_before;

    while (samples_never_seen_before.size() < 10000) {
        samples_never_seen_before.insert(std::rand());
        if (samples_never_seen_before.size() % 10000 == 0) {
            // std::cerr << "Generated " << samples_never_seen_before.size() << std::endl;
        }
    }   
    for (std::set<int>::iterator it = samples_never_seen_before.begin() ; it != samples_never_seen_before.end(); ++it) {
        //std::cout << "find(" << *it << "): " << std::endl;
        dynamic_pgm.find(*it);

    }

    
    return 0;
}
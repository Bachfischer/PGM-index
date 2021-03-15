/*
 * This example shows how to use pgm::DynamicPGMIndex, a std::map-like container supporting inserts and deletes.
 * Compile with:
 *   g++ playground.cpp -std=c++17 -I../include -o playground
 * Run with:
 *   ./playground
 */

#include <vector>
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include "pgm/pgm_index_dynamic.hpp"

int main() {


    // Construct an empty Dynamic PGM-index
    pgm::DynamicPGMIndex<uint32_t, uint32_t> dynamic_pgm{};

    // Insert some data
    dynamic_pgm.insert_or_assign(4, 8);


    // Delete data
    dynamic_pgm.erase(4);

    // Query the container
    std::cout << "find(4) = " << (dynamic_pgm.find(4) == dynamic_pgm.end() ? "not found" : "found") << std::endl;


    
    std::cout << "Number of elements in container: " << dynamic_pgm.size() << std::endl;
    std::cout << "Index size in bytes: " << dynamic_pgm.index_size_in_bytes() << std::endl;
    
    std::cout << "Range search [1, 10000) = " << std::endl;
    
    auto lo = dynamic_pgm.lower_bound(1);
    auto hi = dynamic_pgm.lower_bound(10000);
    for (auto it = lo; it != hi; ++it)
        std::cout << "(" << it->first << "," << it->second << "), ";

    return 0;
}
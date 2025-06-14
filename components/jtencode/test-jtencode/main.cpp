#include "RSEncoder.h"
#include <iostream>
#include <vector>
#include <string>
#include <iomanip> // For std::hex, std::setw, std::setfill

int main() {
  std::cout << "Starting RSEncoder Tests..." << std::endl;

  try {
    // Test case 1: Basic RS(7,3) example (e.g., from tutorials or common usage)
    // symsize=3 (2^3-1 = 7 symbols max)
    // gfpoly=0x7 (primitive polynomial for GF(8): x^3 + x + 1)
    // fcr=1 (first consecutive root)
    // prim=1 (primitive element)
    // nroots=4 (number of parity symbols) -> for RS(7,3), 3 data + 4 parity = 7 total.
    // pad=0
    std::cout << "\n--- Test Case 1: RS(7,3) Encoding ---" << std::endl;
    RSEncoder encoder1(3, 0x7, 1, 1, 4, 0);

    std::vector<uint8_t> data1 = {0x01, 0x02, 0x03}; // 3 data symbols
    std::vector<uint8_t> parity1; // Will be resized to nroots (4)

    encoder1.encode(data1, parity1);

    std::cout << "Input Data (hex): ";
    for (uint8_t b : data1) {
      std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b << " ";
    }
    std::cout << std::endl;

    std::cout << "Generated Parity (hex): ";
    for (uint8_t b : parity1) {
      std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b << " ";
    }
    std::cout << std::endl;
    // Expected parity values for GF(8) RS(7,3) with these parameters can be verified against a known good RS encoder.

    // Test case 2: Character encoding using jt_code
    std::cout << "\n--- Test Case 2: jt_code Character Mapping ---" << std::endl;
    std::cout << "jt_code('5'): " << (int)RSEncoder::jt_code('5') << std::endl;      // Expected: 5
    std::cout << "jt_code('B'): " << (int)RSEncoder::jt_code('B') << std::endl;      // Expected: 11
    std::cout << "jt_code(' '): " << (int)RSEncoder::jt_code(' ') << std::endl;      // Expected: 36
    std::cout << "jt_code('+'): " << (int)RSEncoder::jt_code('+') << std::endl;      // Expected: 37
    std::cout << "jt_code('X'): " << (int)RSEncoder::jt_code('X') << std::endl;      // Expected: 33
    std::cout << "jt_code('~'): " << (int)RSEncoder::jt_code('~') << std::endl;      // Expected: 255 (error code)
    std::cout << "jt_code('a'): " << (int)RSEncoder::jt_code('a') << std::endl;      // Expected: 255 (error code, lowercase)


    // Test case 3: Error handling for constructor
    std::cout << "\n--- Test Case 3: Constructor Error Handling ---" << std::endl;
    try {
      RSEncoder invalidEncoder(0, 0, 0, 0, 0, 0); // Invalid symsize
    } catch (const std::runtime_error& e) {
      std::cout << "Caught expected exception: " << e.what() << std::endl;
    }

  } catch (const std::exception& e) {
    std::cerr << "An unexpected error occurred: " << e.what() << std::endl;
    return 1;
  }

  std::cout << "\nAll tests completed." << std::endl;
  return 0;
}

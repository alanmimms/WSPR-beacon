#include "RSEncode.h"
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>  // For std::hex, std::setw, std::setfill
#include <algorithm> // For std::all_of

// Helper function to print vectors in hex
void printVector(const std::string& label, const std::vector<uint8_t>& vec) {
  std::cout << label << " (hex): ";
  if (vec.empty()) {
    std::cout << "[EMPTY]";
  } else {
    for (uint8_t b : vec) {
      std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b) << " ";
    }
  }
  std::cout << std::endl;
}

int main() {
  std::cout << "Starting RSEncode Tests..." << std::endl;

  // --- Test Case 1: Basic RS(7,3) Encoding (symsize=3, nn=7, nroots=4) ---
  // Parameters: symsize=3, gfpoly=0x7 -> CORRECTED TO 0xB, fcr=1, prim=1, nroots=4, pad=0
  // Message length (nn - nroots - pad) = 7 - 4 - 0 = 3 data symbols
  std::cout << "\n--- Test Case 1: Basic RS(7,3) Encoding ---" << std::endl;
  try {
    // Corrected gfpoly from 0x7 to 0xB (x^3 + x + 1 is primitive for GF(8))
    RSEncode encode1(3, 0xB, 1, 1, 4, 0); // RS(7,3) -> 3 data, 4 parity

    std::vector<uint8_t> data1 = {0x01, 0x02, 0x03}; // Exactly 3 data symbols
    std::vector<uint8_t> parity1; // Will be resized by encode

    encode1.encode(data1, parity1);

    printVector("Input Data", data1);
    printVector("Generated Parity", parity1);
    std::cout << "Parity size: " << parity1.size() << std::endl;
    // Expected parity values for GF(8) RS(7,3) with these parameters can be verified.
    // E.g., for {01 02 03}, parity might be {31 01 04 77} with Phil Karn's example values
    // (values can differ based on specific GF implementation and generator polynomial derivation).

  } catch (const std::exception& e) {
    std::cerr << "Test Case 1 failed: " << e.what() << std::endl;
    return 1;
  }

  // --- Test Case 2: Encoding with empty data ---
  std::cout << "\n--- Test Case 2: Encoding with Empty Data ---" << std::endl;
  try {
    // Corrected gfpoly from 0x7 to 0xB
    RSEncode encode2(3, 0xB, 1, 1, 4, 0);
    std::vector<uint8_t> emptyData = {};
    std::vector<uint8_t> parity2;

    encode2.encode(emptyData, parity2); // Should produce all-zero parity for empty data

    printVector("Input Data", emptyData);
    printVector("Generated Parity", parity2);
    // Expecting 4 zero parity symbols
    bool allZeros = std::all_of(parity2.begin(), parity2.end(), [](uint8_t b){ return b == 0; });
    std::cout << "Parity size: " << parity2.size() << ", All zeros: " << (allZeros ? "true" : "false") << std::endl;

  } catch (const std::exception& e) {
    std::cerr << "Test Case 2 failed: " << e.what() << std::endl;
    return 1;
  }

  // --- Test Case 3: Encoding with padded block (RS(15,10) with 2 pad) ---
  // symsize=4 (2^4-1 = 15 symbols max)
  // gfpoly=0x13 (x^4 + x + 1) -> This is a correct primitive polynomial for GF(16)
  // nroots=5 (15-10=5) -> 10 data symbols, 5 parity symbols
  // pad=2 -> actual data symbols = 15 - 5 - 2 = 8
  std::cout << "\n--- Test Case 3: Encoding with Padding (RS(15,8) data, 5 parity) ---" << std::endl;
  try {
    RSEncode encode3(4, 0x13, 0, 1, 5, 2); // Parameters for GF(16)

    std::vector<uint8_t> data3 = {0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8}; // 8 data symbols
    std::vector<uint8_t> parity3;

    encode3.encode(data3, parity3);

    printVector("Input Data", data3);
    printVector("Generated Parity", parity3);
    std::cout << "Parity size: " << parity3.size() << std::endl;
    // Expected parity size should be 5.

  } catch (const std::exception& e) {
    std::cerr << "Test Case 3 failed: " << e.what() << std::endl;
    return 1;
  }

  // --- Test Case 4: Data vector too long (should still encode based on num_data_symbols) ---
  std::cout << "\n--- Test Case 4: Data Vector Too Long (truncation check) ---" << std::endl;
  try {
    // Corrected gfpoly from 0x7 to 0xB
    RSEncode encode4(3, 0xB, 1, 1, 4, 0); // Expects 3 data symbols
    std::vector<uint8_t> longData = {0x10, 0x11, 0x12, 0x13, 0x14}; // 5 symbols, only first 3 should be used
    std::vector<uint8_t> parity4;

    encode4.encode(longData, parity4); // Only the first (nn - nroots - pad) bytes will be used

    printVector("Input Data (full)", longData);
    printVector("Generated Parity", parity4);
    // Accessing nn, nroots, pad using getter methods
    std::cout << "Note: Only first " << (encode4.getNN() - encode4.getNRoots() - encode4.getPad()) << " data symbols should have been used for encoding." << std::endl;

  } catch (const std::exception& e) {
    std::cerr << "Test Case 4 failed: " << e.what() << std::endl;
    return 1;
  }

  // --- Test Case 5: Data vector too short (should encode, padding with implied zeros) ---
  std::cout << "\n--- Test Case 5: Data Vector Too Short ---" << std::endl;
  try {
    // Corrected gfpoly from 0x7 to 0xB
    RSEncode encode5(3, 0xB, 1, 1, 4, 0); // Expects 3 data symbols
    std::vector<uint8_t> shortData = {0x0A}; // Only 1 symbol
    std::vector<uint8_t> parity5;

    encode5.encode(shortData, parity5); // Will encode 1 data symbol, remaining implied as zeros

    printVector("Input Data (full)", shortData);
    printVector("Generated Parity", parity5);
    std::cout << "Note: Remaining " << (encode5.getNN() - encode5.getNRoots() - encode5.getPad() - shortData.size())
              << " data symbols assumed zero for encoding." << std::endl;

  } catch (const std::exception& e) {
    std::cerr << "Test Case 5 failed: " << e.what() << std::endl;
    return 1;
  }

  // --- Test Case 6: Constructor Error Handling for Invalid Parameters ---
  std::cout << "\n--- Test Case 6: Constructor Error Handling (Invalid Params) ---" << std::endl;
  // Test invalid symsize
  try {
    std::cout << "  Testing symsize = 0: ";
    RSEncode invalidEncode(0, 0xB, 1, 1, 4, 0); // Corrected gfpoly
    std::cout << "ERROR: Expected exception, but none thrown." << std::endl;
  } catch (const std::runtime_error& e) {
    std::cout << "Caught expected exception: " << e.what() << std::endl;
  }
  // Test invalid fcr
  try {
    std::cout << "  Testing fcr = -1: ";
    RSEncode invalidEncode(3, 0xB, -1, 1, 4, 0); // Corrected gfpoly
    std::cout << "ERROR: Expected exception, but none thrown." << std::endl;
  } catch (const std::runtime_error& e) {
    std::cout << "Caught expected exception: " << e.what() << std::endl;
  }
  // Test non-primitive polynomial (now using 0x7, which is NOT primitive for symsize 3)
  // For symsize 3, GF(8), 0x7 is x^3 + x^2 + x + 1, which is reducible (e.g., (x+1)(x^2+1)).
  // This should now correctly throw.
  try {
    std::cout << "  Testing non-primitive gfpoly (0x7 for symsize 3): ";
    RSEncode invalidEncode(3, 0x7, 1, 1, 4, 0); // This should now correctly throw
    std::cout << "ERROR: Expected exception for non-primitive polynomial, but none thrown." << std::endl;
  } catch (const std::runtime_error& e) {
    std::cout << "Caught expected exception: " << e.what() << std::endl;
  }
  // Test invalid prim (e.g., 0)
  try {
    std::cout << "  Testing prim = 0: ";
    RSEncode invalidEncode(3, 0xB, 1, 0, 4, 0); // Corrected gfpoly
    std::cout << "ERROR: Expected exception, but none thrown." << std::endl;
  } catch (const std::runtime_error& e) {
    std::cout << "Caught expected exception: " << e.what() << std::endl;
  }
  // Test invalid nroots (e.g., too high)
  try {
    std::cout << "  Testing nroots > symsize (e.g., 9 for symsize 3): ";
    RSEncode invalidEncode(3, 0xB, 1, 1, 9, 0); // Corrected gfpoly
    std::cout << "ERROR: Expected exception, but none thrown." << std::endl;
  } catch (const std::runtime_error& e) {
    std::cout << "Caught expected exception: " << e.what() << std::endl;
  }
  // Test invalid pad (e.g., too high)
  try {
    std::cout << "  Testing pad too high (e.g., 5 for RS(7,3) so 3 data, pad=5): ";
    RSEncode invalidEncode(3, 0xB, 1, 1, 4, 5); // Corrected gfpoly
    std::cout << "ERROR: Expected exception, but none thrown." << std::endl;
  } catch (const std::runtime_error& e) {
    std::cout << "Caught expected exception: " << e.what() << std::endl;
  }

  std::cout << "\nAll RSEncode tests completed." << std::endl;
  return 0;
}

#include "jtencode-util.h" // Include the utility header
#include "RSEncode.h"     // Include RSEncode if you plan to use it for component-level tests
#include <iostream>
#include <string>
#include <vector>
#include <cassert> // For simple assertions in tests
#include <iomanip> // For std::hex, std::setw, std::setfill

// Helper function to run a test and print results for jtCode
void runJtCodeTest(const std::string& testName, char inputChar, uint8_t expectedOutput) {
  uint8_t actualOutput = jtCode(inputChar);
  std::cout << "  Test: " << testName << " ('" << inputChar << "') -> Expected: "
            << static_cast<int>(expectedOutput) << ", Actual: "
            << static_cast<int>(actualOutput);

  if (actualOutput == expectedOutput) {
    std::cout << " [PASS]" << std::endl;
  } else {
    std::cout << " [FAIL]" << std::endl;
  }
  // Optional: Use assert for hard failures in automated testing
  // assert(actualOutput == expectedOutput);
}

// Helper function to print vectors in hex (copied from rsencode-test-main for consistency)
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
  std::cout << "Starting JTEncode Component Tests..." << std::endl;

  // --- Section 1: jtCode Utility Tests ---
  std::cout << "\n--- Section 1: jtCode Utility Tests ---" << std::endl;

  // Test Case 1.1: Digits ('0' - '9')
  std::cout << "\n--- Test Case 1.1: Digits ---" << std::endl;
  for (char c = '0'; c <= '9'; ++c) {
    runJtCodeTest("Digit " + std::string(1, c), c, static_cast<uint8_t>(c - '0'));
  }

  // Test Case 1.2: Uppercase Letters ('A' - 'Z')
  std::cout << "\n--- Test Case 1.2: Uppercase Letters ---" << std::endl;
  for (char c = 'A'; c <= 'Z'; ++c) {
    runJtCodeTest("Uppercase " + std::string(1, c), c, static_cast<uint8_t>(c - 'A' + 10));
  }

  // Test Case 1.3: Specific Special Characters
  std::cout << "\n--- Test Case 1.3: Specific Special Characters ---" << std::endl;
  runJtCodeTest("Space", ' ', 36);
  runJtCodeTest("Plus", '+', 37);
  runJtCodeTest("Minus", '-', 38);
  runJtCodeTest("Period", '.', 39);
  runJtCodeTest("Slash", '/', 40);
  runJtCodeTest("Question Mark", '?', 41);

  // Test Case 1.4: Invalid/Unhandled Characters (should return 255)
  std::cout << "\n--- Test Case 1.4: Invalid/Unhandled Characters ---" << std::endl;
  runJtCodeTest("Lowercase 'a'", 'a', 255);
  runJtCodeTest("Lowercase 'z'", 'z', 255);
  runJtCodeTest("Exclamation Mark '!'", '!', 255);
  runJtCodeTest("At Symbol '@'", '@', 255);
  runJtCodeTest("Hash '#'", '#', 255);
  runJtCodeTest("Newline '\\n'", '\n', 255);
  runJtCodeTest("Tab '\\t'", '\t', 255);
  runJtCodeTest("Null char '\\0'", '\0', 255);
  runJtCodeTest("Arbitrary unmapped char '~'", '~', 255);
  runJtCodeTest("Arbitrary unmapped char '{'", '{', 255);


  // --- Section 2: Placeholder for Higher-Level Component Tests (e.g., WSPR message encoding) ---
  // If you later implement WSPR message construction logic that uses RSEncode and jtCode,
  // you would add those tests here.
  std::cout << "\n--- Section 2: Higher-Level Component Tests (Placeholder) ---" << std::endl;
  try {
    // Example: If you had a WSPR_Encoder class that internally uses RSEncode and jtCode
    // WSPR_Encoder wsprEncoder;
    // std::vector<uint8_t> wsprMessage = wsprEncoder.encodeMessage("CALLSIGN", "LOC", 10, "MSG");
    // printVector("WSPR Message", wsprMessage);

    // As a simple example, you could test an RSEncode instance here,
    // just to demonstrate that RSEncode can be used within a component test.
    std::cout << "  Demonstrating RSEncode usage within component test:" << std::endl;
    RSEncode exampleEncode(3, 0xB, 1, 1, 4, 0);
    std::vector<uint8_t> exampleData = {0xAA, 0xBB, 0xCC};
    std::vector<uint8_t> exampleParity;
    exampleEncode.encode(exampleData, exampleParity);
    printVector("  Example RSEncode Data", exampleData);
    printVector("  Example RSEncode Parity", exampleParity);

  } catch (const std::exception& e) {
    std::cerr << "Higher-level component test failed: " << e.what() << std::endl;
    return 1;
  }


  std::cout << "\nAll JTEncode Component Tests completed." << std::endl;
  return 0;
}

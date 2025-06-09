#include "JTEncode.h"
#include <iostream>
#include <iomanip> // For std::setw

/**
 * @brief A simple test program to demonstrate the WSPREncoder class.
 */
int main() {
  std::cout << "--- WSPREncoder Test Program ---" << std::endl;

  // 1. Define the WSPR message content.
  const char* callsign = "K1ABC";
  const char* locator = "FN42";
  const int8_t powerDbm = 37; // 37 dBm is approx. 5 Watts

  std::cout << "Encoding message:" << std::endl;
  std::cout << "  Callsign: " << callsign << std::endl;
  std::cout << "  Locator:  " << locator << std::endl;
  std::cout << "  Power:    " << (int)powerDbm << " dBm" << std::endl;
  std::cout << std::endl;

  // 2. Instantiate the WSPREncoder.
  WSPREncoder wsprEncoder;

  // 3. Call the encode method to perform the encoding.
  wsprEncoder.encode(callsign, locator, powerDbm);

  // 4. Print the results. The encoded data is now available in the
  //    public members of the wsprEncoder object.
  std::cout << "Encoding Complete." << std::endl;
  std::cout << "  TX Frequency: " << wsprEncoder.txFreq << " Hz" << std::endl;
  std::cout << "  Symbol Period: " << WSPREncoder::SymbolPeriod << " ms" << std::endl;
  std::cout << std::endl;
  
  std::cout << "Encoded WSPR Symbols (162 total):" << std::endl;

  // Print all 162 symbols in a formatted grid
  for (int i = 0; i < WSPREncoder::TxBufferSize; ++i) {
    // Each symbol is a value from 0-3, representing one of four tones.
    std::cout << (int)wsprEncoder.symbols[i] << " ";
    if ((i + 1) % 18 == 0) {
      std::cout << std::endl; // Newline every 18 symbols
    }
  }

  std::cout << "\n--- Test Complete ---" << std::endl;

  return 0;
}

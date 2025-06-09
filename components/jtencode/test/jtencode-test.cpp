#include "JTEncode.h"
#include <iostream>
#include <cstring>

// A template function to print symbols from any encoder type
template<uint16_t T_S, uint16_t S_P, uint32_t D_F, int T_B>
void printSymbols(JTEncoder<T_S, S_P, D_F, T_B>* encoder, const char* modeName) {
  if (!encoder) return;
  std::cout << "--- " << modeName << " Test ---" << std::endl;
  std::cout << "TX Frequency: " << encoder->txFreq << " Hz" << std::endl;
  std::cout << "Symbol Count: " << encoder->TxBufferSize << std::endl;
  
  // Print all symbols in a formatted grid
  for (int i = 0; i < encoder->TxBufferSize; ++i) {
    std::cout << (int)encoder->symbols[i] << " ";
    if ((i + 1) % 20 == 0) {
      std::cout << std::endl;
    }
  }
  std::cout << "\n\n";
}

void printUsage() {
  std::cout << "Usage: ./jtencode-test <mode>\n"
            << "Available modes: wspr, ft8, jt65, jt9, jt4\n";
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    printUsage();
    return 1;
  }

  const char* mode = argv[1];

  if (strcmp(mode, "wspr") == 0) {
    WSPREncoder wsprEncoder;
    wsprEncoder.encode("K1ABC", "FN42", 37);
    printSymbols(&wsprEncoder, "WSPR");
  } else if (strcmp(mode, "ft8") == 0) {
    FT8Encoder ft8Encoder;
    ft8Encoder.encode("CQ K1ABC FN42");
    printSymbols(&ft8Encoder, "FT8");
  } else if (strcmp(mode, "jt65") == 0) {
    JT65Encoder jt65Encoder;
    jt65Encoder.encode("CQ K1ABC FN42");
    printSymbols(&jt65Encoder, "JT65");
  } else if (strcmp(mode, "jt9") == 0) {
    JT9Encoder jt9Encoder;
    jt9Encoder.encode("CQ K1ABC FN42");
    printSymbols(&jt9Encoder, "JT9");
  } else if (strcmp(mode, "jt4") == 0) {
    JT4Encoder jt4Encoder;
    jt4Encoder.encode("CQ K1ABC FN42");
    printSymbols(&jt4Encoder, "JT4");
  } else {
    std::cout << "Error: Unknown mode '" << mode << "'\n";
    printUsage();
    return 1;
  }

  return 0;
}

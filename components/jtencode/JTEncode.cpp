#include "JTEncode.h"
#include <cstring> // For strncpy and memset

// --- Internal helper functions ---
// (These are unchanged)
static uint8_t wsprCode(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'A' && c <= 'Z') {
    return c - 'A' + 10;
  }
  return 36; // Space character
}

static uint32_t nhash(const int* d) {
  uint32_t h = 0x811c9dc5; // FNV-1a 32-bit offset basis
  for (int i = 0; i < 2; ++i) {
    h ^= (uint32_t)d[i];
    h *= 0x01000193; // FNV-1a 32-bit prime
  }
  return h;
}

// --- WSPREncoder Implementation ---

void WSPREncoder::encode(const char* callsign, const char* locator, int8_t powerDbm) {
  // 1. Store message data
  strncpy(this->callsign, callsign, 11);
  this->callsign[11] = '\0';
  strncpy(this->locator, locator, 6);
  this->locator[6] = '\0';
  this->powerDbm = powerDbm;

  // 2. Run the WSPR encoding pipeline
  packBits();
  convolveSymbols();
  interleave();
}

void WSPREncoder::packBits() {
  // This function was correct and remains unchanged.
  memset(packedData, 0, sizeof(packedData));
  uint64_t n = 0;

  uint32_t nCall = 0;
  nCall += wsprCode(callsign[0]) * 36 * 36 * 36 * 36 * 10;
  nCall += wsprCode(callsign[1]) * 36 * 36 * 36 * 10;
  nCall += wsprCode(callsign[2]) * 36 * 36 * 10;
  nCall += wsprCode(callsign[3]) * 36 * 10;
  nCall += wsprCode(callsign[4]) * 10;
  nCall += wsprCode(callsign[5]);
  n = nCall;

  uint16_t nLoc = 179 - (locator[0] - 'A') * 10 - (locator[1] - 'A');
  nLoc *= 100;
  nLoc += (locator[2] - '0') * 10 + (locator[3] - '0');
  n = (n << 15) | nLoc;

  uint8_t nPow = (powerDbm >= 0 && powerDbm <= 60) ? (uint8_t)powerDbm : 63;
  n = (n << 7) | nPow;

  for (int i = 0; i < 50; ++i) {
    if ((n >> (49 - i)) & 1) {
      packedData[i / 8] |= (1 << (7 - (i % 8)));
    }
  }
}

// -----------------------------------------------------------------------------
// THIS IS THE CORRECTED FUNCTION
// -----------------------------------------------------------------------------
void WSPREncoder::convolveSymbols() {
  // WSPR uses a k=32, r=1/2 non-recursive convolutional code.
  const uint32_t g1 = 0xF2D05351;
  const uint32_t g2 = 0xE4613C47;

  // Create a buffer large enough for the message bits plus the filter lookahead (50+31),
  // padded with zeros for flushing.
  uint8_t messageBuffer[205] = {0};

  // Unpack the 50 message bits from packedData into the buffer
  for (int i = 0; i < 50; ++i) {
    messageBuffer[i] = (packedData[i / 8] >> (7 - (i % 8))) & 1;
  }

  // Perform convolution using a FIR filter model
  for (int i = 0; i < 162; ++i) {
    uint8_t bit1 = 0;
    uint8_t bit2 = 0;
    // Dot product of the generator polynomials with the message window
    for (int j = 0; j < 32; ++j) {
      // Check the j-th bit of the generator polynomial
      if (((g1 >> j) & 1) != 0) {
        bit1 ^= messageBuffer[i + j];
      }
      if (((g2 >> j) & 1) != 0) {
        bit2 ^= messageBuffer[i + j];
      }
    }
    symbols[i] = (bit1 << 1) | bit2;
  }
}

void WSPREncoder::interleave() {
  // This function was correct and remains unchanged.
  uint8_t temporaryBuffer[TxBufferSize];
  int d[2] = {0, 0};

  for (int i = 0; i < TxBufferSize; ++i) {
    d[0] = i;
    uint32_t h = nhash(d);
    temporaryBuffer[i] = symbols[h % 162];
  }
  
  memcpy(symbols, temporaryBuffer, TxBufferSize);
}

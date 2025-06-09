#include "JTEncode.h"
#include <cstring> // For strncpy and memset

// --- Internal helper functions moved from separate files ---

// Converts a character to its 37-value representation for WSPR packing.
// (0-9 maps to 0-9, A-Z maps to 10-35, space maps to 36)
static uint8_t wsprCode(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'A' && c <= 'Z') {
    return c - 'A' + 10;
  }
  return 36; // Space character
}

// Hashing function for WSPR interleaver, moved from nhash.h
// A simple implementation based on the one in WSJT-X.
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
  this->callsign[11] = '\0'; // Ensure null termination
  strncpy(this->locator, locator, 6);
  this->locator[6] = '\0';
  this->powerDbm = powerDbm;

  // 2. Run the WSPR encoding pipeline
  packBits();
  convolveSymbols();
  interleave();
}

void WSPREncoder::packBits() {
  memset(packedData, 0, sizeof(packedData));
  uint64_t n = 0;

  // Pack callsign (6 chars, 28 bits)
  uint32_t nCall = 0;
  nCall += wsprCode(callsign[0]) * 36 * 36 * 36 * 36 * 10;
  nCall += wsprCode(callsign[1]) * 36 * 36 * 36 * 10;
  nCall += wsprCode(callsign[2]) * 36 * 36 * 10;
  nCall += wsprCode(callsign[3]) * 36 * 10;
  nCall += wsprCode(callsign[4]) * 10;
  nCall += wsprCode(callsign[5]);
  n = nCall;

  // Pack locator (4 chars, 15 bits)
  uint16_t nLoc = 179 - (locator[0] - 'A') * 10 - (locator[1] - 'A');
  nLoc *= 100;
  nLoc += (locator[2] - '0') * 10 + (locator[3] - '0');
  n = (n << 15) | nLoc;

  // Pack power (7 bits)
  uint8_t nPow = (powerDbm >= 0 && powerDbm <= 60) ? (uint8_t)powerDbm : 63;
  n = (n << 7) | nPow;

  // Unpack the 50 bits from the 64-bit integer into the byte array
  for (int i = 0; i < 50; ++i) {
    if ((n >> (49 - i)) & 1) {
      packedData[i / 8] |= (1 << (7 - (i % 8)));
    }
  }
}

void WSPREncoder::convolveSymbols() {
  // WSPR uses a k=32, r=1/2 non-recursive convolutional code.
  // Generator polynomials G1 and G2 are standard for WSPR.
  const uint32_t g1 = 0xF2D05351;
  const uint32_t g2 = 0xE4613C47;

  // Create a message buffer with 50 data bits and 31 zero bits for flushing
  uint8_t msg[81] = {0}; 
  for (int i = 0; i < 50; ++i) {
    msg[i] = (packedData[i / 8] >> (7 - (i % 8))) & 1;
  }

  // Perform convolution
  for (int i = 0; i < 162; ++i) {
    uint32_t p1 = (i < 50) ? g1 : (g1 << i-49);
    uint32_t p2 = (i < 50) ? g2 : (g2 << i-49);
    uint8_t bit1 = 0;
    uint8_t bit2 = 0;
    for (int j = 0; j < 32; ++j) {
      if (((p1 >> j) & 1) != 0) bit1 ^= msg[i - j];
      if (((p2 >> j) & 1) != 0) bit2 ^= msg[i - j];
    }
    symbols[i] = (bit1 << 1) | bit2;
  }
}

void WSPREncoder::interleave() {
  uint8_t temporaryBuffer[TxBufferSize];
  int d[2] = {0, 0};

  // Permute the symbols based on the hash function
  for (int i = 0; i < TxBufferSize; ++i) {
    d[0] = i;
    uint32_t h = nhash(d);
    temporaryBuffer[i] = symbols[h % 162];
  }
  
  // Copy the interleaved symbols back
  memcpy(symbols, temporaryBuffer, TxBufferSize);
}

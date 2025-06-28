#include "jtencode-util.h" // Include the utility header

// Implementation of the jtCode character mapping utility.
// This function converts a character to a specific integer code
// based on WSPR encoding rules (digits, uppercase letters, special symbols).
// Returns 255 for any unmapped or invalid character.
uint8_t jtCode(char c) {
  if (std::isdigit(static_cast<unsigned char>(c))) {
    // Digits '0'-'9' map to 0-9
    return static_cast<uint8_t>(c - '0');
  } else if (c >= 'A' && c <= 'Z') {
    // Uppercase letters 'A'-'Z' map to 10-35
    return static_cast<uint8_t>(c - 'A' + 10);
  } else {
    // Specific special characters
    switch (c) {
      case ' ': return 36;
      case '+': return 37;
      case '-': return 38;
      case '.': return 39;
      case '/': return 40;
      case '?': return 41;
      default:  return 255; // Error code for unsupported characters
    }
  }
}

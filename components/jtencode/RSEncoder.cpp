#include "RSEncoder.h"
#include <string.h> // For memset, memmove
#include <stdexcept> // For std::runtime_error
#include <numeric>   // Not used in this version
#include <algorithm> // For std::fill (could be used instead of memset for parity)

// Internal definition of the data_t type, now consistent with RSEncoder.
typedef uint8_t data_t;

// Internal macro for A_0 (zero in index form) to maintain consistency with original logic.
// It explicitly takes the 'nn' value from the current rs_control_block instance.
// Now, it's just 'nn'.
#define RS_A_0_VAL(nn_val) (nn_val)


// --- RSEncoder Class Implementation ---

// Private modnn helper
int RSEncoder::modnn_private(int x) const {
  // This method directly uses member variables
  while (x >= nn) {
    x -= nn;
    x = (x >> mm) + (x & nn);
  }
  return x;
}

// Constructor
RSEncoder::RSEncoder(int symsize, int gfpoly, int fcr_param, int prim_param, int nroots_param, int pad_param)
  : mm(symsize), nn((1 << symsize) - 1), nroots(nroots_param), fcr(fcr_param), prim(prim_param), pad(pad_param) {

  // Validate parameters first, throwing exceptions for invalid inputs
  if (mm < 0 || mm > 8 * sizeof(data_t)) {
    throw std::runtime_error("RSEncoder: Invalid symsize (bits per symbol).");
  }
  if (fcr < 0 || fcr >= (1 << mm)) {
    throw std::runtime_error("RSEncoder: Invalid fcr (first consecutive root).");
  }
  if (prim <= 0 || prim >= (1 << mm)) {
    throw std::runtime_error("RSEncoder: Invalid prim (primitive element).");
  }
  if (nroots < 0 || nroots >= (1 << mm)) {
    throw std::runtime_error("RSEncoder: Invalid nroots (number of roots/parity symbols).");
  }
  if (pad < 0 || pad >= ((1 << mm) - 1 - nroots)) {
    throw std::runtime_error("RSEncoder: Invalid pad (padding bytes).");
  }

  // Resize vectors instead of malloc
  alpha_to.resize(nn + 1);
  index_of.resize(nn + 1);
  genpoly.resize(nroots + 1);

  // Generate Galois field lookup tables (alpha_to and index_of)
  index_of[0] = RS_A_0_VAL(nn); // log(zero) = -inf
  alpha_to[RS_A_0_VAL(nn)] = 0; // alpha**-inf = 0
  int sr = 1;
  for (int i = 0; i < nn; ++i) {
    index_of[sr] = i;
    alpha_to[i] = sr;
    sr <<= 1;
    if (sr & (1 << mm)) { // If overflow beyond symsize bits, XOR with gfpoly
      sr ^= gfpoly;
    }
    sr &= nn; // Ensure sr stays within the field size
  }

  // Check if the field generator polynomial is primitive
  if (sr != 1) {
    // Vectors will be automatically destructed, no manual free needed.
    throw std::runtime_error("RSEncoder: Field generator polynomial is not primitive!");
  }

  // Initialize the generator polynomial
  genpoly[0] = 1; // Initialize the first coefficient
  for (int i = 0, root = fcr * prim; i < nroots; ++i, root += prim) {
    genpoly[i + 1] = 1; // Set next coefficient to 1

    // Multiply genpoly[] by (x + alpha^root) in GF(2^m)
    for (int j = i; j > 0; --j) {
      if (genpoly[j] != 0) {
	genpoly[j] = genpoly[j - 1] ^
	  alpha_to[modnn_private(index_of[genpoly[j]] + root)];
      } else {
	genpoly[j] = genpoly[j - 1];
      }
    }
    // genpoly[0] can never be zero after the first root
    genpoly[0] = alpha_to[modnn_private(index_of[genpoly[0]] + root)];
  }

  // Convert genpoly[] to index form for quicker encoding
  for (int i = 0; i <= nroots; ++i) {
    genpoly[i] = index_of[genpoly[i]];
  }
}

// Destructor (trivial due to std::vector handling memory)
RSEncoder::~RSEncoder() {
  // No explicit cleanup needed for std::vector members; they clean themselves up.
}

// Move constructor implementation
RSEncoder::RSEncoder(RSEncoder&& other) noexcept
  : mm(std::move(other.mm)),
    nn(std::move(other.nn)),
    nroots(std::move(other.nroots)),
    fcr(std::move(other.fcr)),
    prim(std::move(other.prim)),
    iprim(std::move(other.iprim)),
    pad(std::move(other.pad)),
    alpha_to(std::move(other.alpha_to)),
    index_of(std::move(other.index_of)),
    genpoly(std::move(other.genpoly)) {
  // Basic types like int are copied, std::vector is moved
}

// Move assignment operator implementation
RSEncoder& RSEncoder::operator=(RSEncoder&& other) noexcept {
  if (this != &other) { // Handle self-assignment
    // Move members from 'other' to 'this'
    mm = std::move(other.mm);
    nn = std::move(other.nn);
    nroots = std::move(other.nroots);
    fcr = std::move(other.fcr);
    prim = std::move(other.prim);
    iprim = std::move(other.iprim);
    pad = std::move(other.pad);
    alpha_to = std::move(other.alpha_to);
    index_of = std::move(other.index_of);
    genpoly = std::move(other.genpoly);
  }
  return *this;
}

// Encode method implementation
void RSEncoder::encode(const std::vector<uint8_t>& data, std::vector<uint8_t>& parity) const {
  // Check if the encoder is properly initialized (though constructor should ensure this)
  if (alpha_to.empty() || index_of.empty() || genpoly.empty()) {
    throw std::runtime_error("RSEncoder: Encoder internal tables not initialized. Cannot encode.");
  }

  // Ensure parity vector is correctly sized
  if (parity.size() != static_cast<size_t>(nroots)) {
    parity.resize(nroots);
  }

  // Get raw pointers to vector data for compatibility with memset/memmove
  // and direct array-like access for performance.
  data_t* parity_ptr = parity.data();
  const data_t* data_ptr = data.data();

  // Initialize parity symbols to zero
  memset(parity_ptr, 0, nroots * sizeof(data_t));

  // Encoding loop based on the original encode_rs.h logic
  // Number of data symbols to process
  const int num_data_symbols = nn - nroots - pad;

  for (int i = 0; i < num_data_symbols; ++i) {
    // Calculate feedback term: data_symbol XOR first_parity_symbol, then convert to index form
    data_t feedback = index_of[data_ptr[i] ^ parity_ptr[0]];

    if (feedback != RS_A_0_VAL(nn)) { // If feedback term is non-zero
#ifdef UNNORMALIZED
      // This block is typically for unnormalized generator polynomials.
      // As per original code's comment, it's often unnecessary when genpoly[nroots] is unity.
      // feedback = modnn_private(nn - genpoly[nroots] + feedback);
#endif
      // Apply feedback to the rest of the parity symbols
      for (int j = 1; j < nroots; ++j) {
	parity_ptr[j] ^= alpha_to[modnn_private(feedback + genpoly[nroots - j])];
      }
    }

    // Shift parity registers: move all elements one position to the left
    memmove(&parity_ptr[0], &parity_ptr[1], sizeof(data_t) * (nroots - 1));

    // Calculate the last parity symbol based on feedback
    if (feedback != RS_A_0_VAL(nn)) {
      parity_ptr[nroots - 1] = alpha_to[modnn_private(feedback + genpoly[0])];
    } else {
      parity_ptr[nroots - 1] = 0;
    }
  }
}

#ifndef RSENCODE_H
#define RSENCODE_H

#include <cstdint>   // For uint8_t
#include <vector>    // Using std::vector for dynamic arrays
#include <string>    // For potential error messages or char conversion utilities
#include <stdexcept> // For exceptions
#include <cctype>    // For isdigit, isupper
#include <utility>   // For std::move

/**
 * @class RSEncode
 * @brief A C++ class for Reed-Solomon encoding.
 *
 * This class encapsulates the Reed-Solomon encoding functionality,
 * managing memory and providing a robust, object-oriented interface.
 * It replaces the raw C functions and global data structures with
 * proper C++ class members and methods.
 */
class RSEncode {
public:
  /**
   * @brief Constructor: Initializes the Reed-Solomon encoder.
   * @param symsize Bits per symbol (e.g., 8 for 256-symbol Galois field).
   * @param gfpoly Generator polynomial for the Galois field.
   * @param fcr First consecutive root, index form.
   * @param prim Primitive element, index form.
   * @param nroots Number of generator roots (number of parity symbols).
   * @param pad Padding bytes in shortened block.
   * @throws std::runtime_error if initialization parameters are invalid or
   * memory allocation fails, or if the GF polynomial is not primitive.
   */
  RSEncode(int symsize, int gfpoly, int fcr, int prim, int nroots, int pad);

  /**
   * @brief Destructor: Frees all dynamically allocated memory.
   *
   * Adheres to RAII (Resource Acquisition Is Initialization) principle.
   * With std::vector members, this destructor becomes trivial as vector
   * handles its own memory.
   */
  ~RSEncode();

  // Prevent copying to avoid shallow copy issues with raw pointers.
  RSEncode(const RSEncode&) = delete;
  RSEncode& operator=(const RSEncode&) = delete;

  // Enable move semantics for efficient resource transfer (C++11 and later).
  RSEncode(RSEncode&& other) noexcept;
  RSEncode& operator=(RSEncode&& other) noexcept;

  /**
   * @brief Encodes input data and generates parity symbols.
   * @param data A constant reference to a vector of input data symbols.
   * @param parity A reference to a vector where the generated parity symbols
   * will be stored. It will be resized to `nroots`.
   * @throws std::runtime_error if the encoder is not initialized.
   */
  void encode(const std::vector<uint8_t>& data, std::vector<uint8_t>& parity) const;

  // --- Public Getter Methods for Test/Read-Only Access ---
  /**
   * @brief Get the total number of symbols per block.
   * @return The value of nn.
   */
  int getNN() const { return nn; }

  /**
   * @brief Get the number of generator roots (parity symbols).
   * @return The value of nroots.
   */
  int getNRoots() const { return nroots; }

  /**
   * @brief Get the number of padding bytes in a shortened block.
   * @return The value of pad.
   */
  int getPad() const { return pad; }

private:
  // Core Reed-Solomon parameters
  int mm;              /**< Bits per symbol */
  int nn;              /**< Symbols per block (= (1<<mm)-1) */
  int nroots;          /**< Number of generator roots = number of parity symbols */
  int fcr;             /**< First consecutive root, index form */
  int prim;            /**< Primitive element, index form */
  int iprim;           /**< prim-th root of 1, index form */
  int pad;             /**< Padding bytes in shortened block */

  // Galois field lookup tables and generator polynomial (dynamically sized, now std::vector)
  std::vector<uint8_t> alpha_to;    /**< log lookup table */
  std::vector<uint8_t> index_of;    /**< Antilog lookup table */
  std::vector<uint8_t> genpoly;     /**< Generator polynomial */


  /**
   * @brief Private helper function for modulo operation within the Galois field.
   * @param x The value to reduce.
   * @return The reduced value modulo `nn`.
   */
  int modnn_private(int x) const;
};

#endif // RSENCODE_H

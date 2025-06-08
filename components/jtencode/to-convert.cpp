/*
 * JTEncode.cpp - JT65/JT9/WSPR/FSQ encoder library
 *
 * Copyright (C) 2015-2021 Jason Milldrum <milldrum@gmail.com>
 * Copyright (C) 2025 Alan Mimms <alanmimms@gmail.com>
 *
 * Based on the algorithms presented in the WSJT software suite.
 * Thanks to Andy Talbot G4JNT for the whitepaper on the WSPR encoding
 * process that helped me to understand all of this.
 *
 * This code has been brutally hacked by Alan Mimms WB7NAB to use
 * modern C++ (C++17 I think) to improve readability, clarity, and
 * modularity. Its target-specific nature has been eliminated and
 * replaced by an interface that can be implemented for any given
 * target to make it more broadly applicable to try to limit rampant
 * forkulation that has been common for code in the past.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <JTEncode.h>
#include <crc14.h>
#include <nhash.h>

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

// Define an upper bound on the number of glyphs.  Defining it this
// way allows adding characters without having to update a hard-coded
// upper bound.
#define NGLYPHS         (sizeof(fsq_code_table)/sizeof(fsq_code_table[0]))


// Define the structure of a varicode table
typedef struct fsq_varicode
{
    uint8_t ch;
    uint8_t var[2];
} Varicode;

// The FSQ varicode table, based on the FSQ Varicode V3.0
// document provided by Murray Greenman, ZL1BPU

static const Varicode fsq_code_table[] = {
  {' ', {00, 00}}, // space
  {'!', {11, 30}},
  {'"', {12, 30}},
  {'#', {13, 30}},
  {'$', {14, 30}},
  {'%', {15, 30}},
  {'&', {16, 30}},
  {'\'', {17, 30}},
  {'(', {18, 30}},
  {')', {19, 30}},
  {'*', {20, 30}},
  {'+', {21, 30}},
  {',', {27, 29}},
  {'-', {22, 30}},
  {'.', {27, 00}},
  {'/', {23, 30}},
  {'0', {10, 30}},
  {'1', {01, 30}},
  {'2', {02, 30}},
  {'3', {03, 30}},
  {'4', {04, 30}},
  {'5', {05, 30}},
  {'6', {06, 30}},
  {'7', {07, 30}},
  {'8', {8, 30}},
  {'9', {9, 30}},
  {':', {24, 30}},
  {';', {25, 30}},
  {'<', {26, 30}},
  {'=', {00, 31}},
  {'>', {27, 30}},
  {'?', {28, 29}},
  {'@', {00, 29}},
  {'A', {01, 29}},
  {'B', {02, 29}},
  {'C', {03, 29}},
  {'D', {04, 29}},
  {'E', {05, 29}},
  {'F', {06, 29}},
  {'G', {07, 29}},
  {'H', {8, 29}},
  {'I', {9, 29}},
  {'J', {10, 29}},
  {'K', {11, 29}},
  {'L', {12, 29}},
  {'M', {13, 29}},
  {'N', {14, 29}},
  {'O', {15, 29}},
  {'P', {16, 29}},
  {'Q', {17, 29}},
  {'R', {18, 29}},
  {'S', {19, 29}},
  {'T', {20, 29}},
  {'U', {21, 29}},
  {'V', {22, 29}},
  {'W', {23, 29}},
  {'X', {24, 29}},
  {'Y', {25, 29}},
  {'Z', {26, 29}},
  {'[', {01, 31}},
  {'\\', {02, 31}},
  {']', {03, 31}},
  {'^', {04, 31}},
  {'_', {05, 31}},
  {'`', {9, 31}},
  {'a', {01, 00}},
  {'b', {02, 00}},
  {'c', {03, 00}},
  {'d', {04, 00}},
  {'e', {05, 00}},
  {'f', {06, 00}},
  {'g', {07, 00}},
  {'h', {8, 00}},
  {'i', {9, 00}},
  {'j', {10, 00}},
  {'k', {11, 00}},
  {'l', {12, 00}},
  {'m', {13, 00}},
  {'n', {14, 00}},
  {'o', {15, 00}},
  {'p', {16, 00}},
  {'q', {17, 00}},
  {'r', {18, 00}},
  {'s', {19, 00}},
  {'t', {20, 00}},
  {'u', {21, 00}},
  {'v', {22, 00}},
  {'w', {23, 00}},
  {'x', {24, 00}},
  {'y', {25, 00}},
  {'z', {26, 00}},
  {'{', {06, 31}},
  {'|', {07, 31}},
  {'}', {8, 31}},
  {'~', {00, 30}},
  {127, {28, 31}}, // DEL
  {13,  {28, 00}}, // CR
  {10,  {28, 00}}, // LF
  {0,   {28, 30}}, // IDLE
  {241, {10, 31}}, // plus/minus
  {246, {11, 31}}, // division sign
  {248, {12, 31}}, // degrees sign
  {158, {13, 31}}, // multiply sign
  {156, {14, 31}}, // pound sterling sign
  {8,   {27, 31}}  // BS
};

static const uint8_t crc8_table[] = {
    0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15, 0x38, 0x3f, 0x36, 0x31,
    0x24, 0x23, 0x2a, 0x2d, 0x70, 0x77, 0x7e, 0x79, 0x6c, 0x6b, 0x62, 0x65,
    0x48, 0x4f, 0x46, 0x41, 0x54, 0x53, 0x5a, 0x5d, 0xe0, 0xe7, 0xee, 0xe9,
    0xfc, 0xfb, 0xf2, 0xf5, 0xd8, 0xdf, 0xd6, 0xd1, 0xc4, 0xc3, 0xca, 0xcd,
    0x90, 0x97, 0x9e, 0x99, 0x8c, 0x8b, 0x82, 0x85, 0xa8, 0xaf, 0xa6, 0xa1,
    0xb4, 0xb3, 0xba, 0xbd, 0xc7, 0xc0, 0xc9, 0xce, 0xdb, 0xdc, 0xd5, 0xd2,
    0xff, 0xf8, 0xf1, 0xf6, 0xe3, 0xe4, 0xed, 0xea, 0xb7, 0xb0, 0xb9, 0xbe,
    0xab, 0xac, 0xa5, 0xa2, 0x8f, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9d, 0x9a,
    0x27, 0x20, 0x29, 0x2e, 0x3b, 0x3c, 0x35, 0x32, 0x1f, 0x18, 0x11, 0x16,
    0x03, 0x04, 0x0d, 0x0a, 0x57, 0x50, 0x59, 0x5e, 0x4b, 0x4c, 0x45, 0x42,
    0x6f, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7d, 0x7a, 0x89, 0x8e, 0x87, 0x80,
    0x95, 0x92, 0x9b, 0x9c, 0xb1, 0xb6, 0xbf, 0xb8, 0xad, 0xaa, 0xa3, 0xa4,
    0xf9, 0xfe, 0xf7, 0xf0, 0xe5, 0xe2, 0xeb, 0xec, 0xc1, 0xc6, 0xcf, 0xc8,
    0xdd, 0xda, 0xd3, 0xd4, 0x69, 0x6e, 0x67, 0x60, 0x75, 0x72, 0x7b, 0x7c,
    0x51, 0x56, 0x5f, 0x58, 0x4d, 0x4a, 0x43, 0x44, 0x19, 0x1e, 0x17, 0x10,
    0x05, 0x02, 0x0b, 0x0c, 0x21, 0x26, 0x2f, 0x28, 0x3d, 0x3a, 0x33, 0x34,
    0x4e, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5c, 0x5b, 0x76, 0x71, 0x78, 0x7f,
    0x6a, 0x6d, 0x64, 0x63, 0x3e, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2c, 0x2b,
    0x06, 0x01, 0x08, 0x0f, 0x1a, 0x1d, 0x14, 0x13, 0xae, 0xa9, 0xa0, 0xa7,
    0xb2, 0xb5, 0xbc, 0xbb, 0x96, 0x91, 0x98, 0x9f, 0x8a, 0x8d, 0x84, 0x83,
    0xde, 0xd9, 0xd0, 0xd7, 0xc2, 0xc5, 0xcc, 0xcb, 0xe6, 0xe1, 0xe8, 0xef,
    0xfa, 0xfd, 0xf4, 0xf3
};

static const uint8_t jt9i[JT9_BIT_COUNT] = {
  0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0x10, 0x90, 0x50, 0x30, 0xb0, 0x70,
  0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0x18, 0x98, 0x58, 0x38, 0xb8, 0x78,
  0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0x14, 0x94, 0x54, 0x34, 0xb4, 0x74,
  0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0x1c, 0x9c, 0x5c, 0x3c, 0xbc, 0x7c,
  0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0x12, 0x92, 0x52, 0x32, 0xb2, 0x72,
  0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0x1a, 0x9a, 0x5a, 0x3a, 0xba, 0x7a,
  0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0x16, 0x96, 0x56, 0x36, 0xb6, 0x76,
  0x0e, 0x8e, 0x4e, 0x2e, 0xae, 0x6e, 0x1e, 0x9e, 0x5e, 0x3e, 0xbe, 0x7e, 0x01,
  0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0x11, 0x91, 0x51, 0x31, 0xb1, 0x71, 0x09,
  0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0x19, 0x99, 0x59, 0x39, 0xb9, 0x79, 0x05,
  0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0x15, 0x95, 0x55, 0x35, 0xb5, 0x75, 0x0d,
  0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0x1d, 0x9d, 0x5d, 0x3d, 0xbd, 0x7d, 0x03,
  0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0x13, 0x93, 0x53, 0x33, 0xb3, 0x73, 0x0b,
  0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0x1b, 0x9b, 0x5b, 0x3b, 0xbb, 0x7b, 0x07,
  0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0x17, 0x97, 0x57, 0x37, 0xb7, 0x77, 0x0f,
  0x8f, 0x4f, 0x2f, 0xaf, 0x6f, 0x1f, 0x9f, 0x5f, 0x3f, 0xbf, 0x7f
};


/* Public Class Members */

JTEncode::JTEncode(void)
{
  // Initialize the Reed-Solomon encoder
  rs_inst = (struct rs *)(intptr_t)init_rs_int(6, 0x43, 3, 1, 51, 0);
  // memset(callsign, 0, 13);
}


static void grayCodeBuffer(uint8_t * g, uint8_t symbol_count) {

  for(int i = 0; i < symbol_count; i++) {
    g[i] = (g[i] >> 1) ^ g[i];
  }
}


/*
 * jt65_encode(const char * message, uint8_t * symbols)
 *
 * Takes an arbitrary message of up to 13 allowable characters and returns
 * a channel symbol table.
 *
 * message - Plaintext Type 6 message.
 * symbols - Array of channel symbols to transmit returned by the method.
 *  Ensure that you pass a uint8_t array of at least size JT65_SYMBOL_COUNT to the method.
 *
 */
void JTEncode::jt65_encode(const char * msg, uint8_t * symbols)
{
  char message[14];
  memset(message, 0, 14);
  strcpy(message, msg);

  // Ensure that the message text conforms to standards
  // --------------------------------------------------
  jt_message_prep(message);

  // Bit packing
  // -----------
  uint8_t c[12];
  jt65_bit_packing(message, c);

  // Reed-Solomon encoding
  // ---------------------
  uint8_t s[JT65_ENCODE_COUNT];
  rs_encode(c, s);

  // Interleaving
  // ------------
  jt65_interleave(s);

  // Gray Code
  // ---------
  grayCodeBuffer(s, JT65_ENCODE_COUNT);

  // Merge with sync vector
  // ----------------------
  jt65_merge_sync_vector(s, symbols);
}

/*
 * jt9_encode(const char * message, uint8_t * symbols)
 *
 * Takes an arbitrary message of up to 13 allowable characters and returns
 * a channel symbol table.
 *
 * message - Plaintext Type 6 message.
 * symbols - Array of channel symbols to transmit returned by the method.
 *  Ensure that you pass a uint8_t array of at least size JT9_SYMBOL_COUNT to the method.
 *
 */
void JTEncode::jt9_encode(const char * msg, uint8_t * symbols)
{
  char message[14];
  memset(message, 0, 14);
  strcpy(message, msg);

  // Ensure that the message text conforms to standards
  // --------------------------------------------------
  jt_message_prep(message);

  // Bit packing
  // -----------
  uint8_t c[13];
  jt9_bit_packing(message, c);

  // Convolutional Encoding
  // ---------------------
  uint8_t s[JT9_BIT_COUNT];
  convolve(c, s, 13, JT9_BIT_COUNT);

  // Interleaving
  // ------------
  jt9_interleave(s);

  // Pack into 3-bit symbols
  // -----------------------
  uint8_t a[JT9_ENCODE_COUNT];
  jt9_packbits(s, a);

  // Gray Code
  // ---------
  grayCodeBuffer(a, JT9_ENCODE_COUNT);

  // Merge with sync vector
  // ----------------------
  jt9_merge_sync_vector(a, symbols);
}

/*
 * jt4_encode(const char * message, uint8_t * symbols)
 *
 * Takes an arbitrary message of up to 13 allowable characters and returns
 * a channel symbol table.
 *
 * message - Plaintext Type 6 message.
 * symbols - Array of channel symbols to transmit returned by the method.
 *  Ensure that you pass a uint8_t array of at least size JT9_SYMBOL_COUNT to the method.
 *
 */
void JTEncode::jt4_encode(const char * msg, uint8_t * symbols)
{
  char message[14];
  memset(message, 0, 14);
  strcpy(message, msg);

  // Ensure that the message text conforms to standards
  // --------------------------------------------------
  jt_message_prep(message);

  // Bit packing
  // -----------
  uint8_t c[13];
  jt9_bit_packing(message, c);

  // Convolutional Encoding
  // ---------------------
  uint8_t s[JT4_SYMBOL_COUNT];
  convolve(c, s, 13, JT4_BIT_COUNT);

  // Interleaving
  // ------------
  jt9_interleave(s);
  memmove(s + 1, s, JT4_BIT_COUNT);
  s[0] = 0; // Append a 0 bit to start of sequence

  // Merge with sync vector
  // ----------------------
  jt4_merge_sync_vector(s, symbols);
}

/*
 * wspr_encode(const char * call, const char * loc, const uint8_t dbm, uint8_t * symbols)
 *
 * Takes a callsign, grid locator, and power level and returns a WSPR symbol
 * table for a Type 1, 2, or 3 message.
 *
 * call - Callsign (12 characters maximum).
 * loc - Maidenhead grid locator (6 characters maximum).
 * dbm - Output power in dBm.
 * symbols - Array of channel symbols to transmit returned by the method.
 *  Ensure that you pass a uint8_t array of at least size WSPR_SYMBOL_COUNT to the method.
 *
 */

void WSPREncoder::encode(const char *callP, const char *locP, const uint8_t dbm) {
  char call_[13];
  char loc_[7];
  uint8_t dbm_ = dbm;
  strcpy(call_, call);
  strcpy(loc_, loc);

  // Ensure that the message text conforms to standards
  // --------------------------------------------------
  wspr_message_prep(call_, loc_, dbm_);

  // Bit packing
  // -----------
  uint8_t c[11];
  wspr_bit_packing(c);

  // Convolutional Encoding
  // ---------------------
  uint8_t s[WSPR_SYMBOL_COUNT];
  convolve(c, s, 11, WSPR_BIT_COUNT);

  // Interleaving
  // ------------
  wspr_interleave(s);

  // Merge with sync vector
  // ----------------------
  wspr_merge_sync_vector(s, symbols);
}


void WSPREncoder::messagePrep(const char *callP, const char *locP, const uint_t dbm) {
  // Callsign validation and padding
  // -------------------------------
	
  // Ensure that the only allowed characters are digits, uppercase letters, slash, and angle brackets
  for (int i = 0; i < 12; i++) {

    if(call[i] != '/' && call[i] != '<' && call[i] != '>') {
      call[i] = toupper(call[i]);

      if(!(isdigit(call[i]) || isupper(call[i]))) {
	call[i] = ' ';
      }
    }
  }

  call[12] = 0;

  strncpy(callsign, call, 12);

  // Grid locator validation
  if(strlen(loc) == 4 || strlen(loc) == 6) {

    for(int i = 0; i <= 1; i++) {
      loc[i] = toupper(loc[i]);
      if((loc[i] < 'A' || loc[i] > 'R')) strncpy(loc, "AA00AA", 7);
    }

    for (int i = 2; i <= 3; i++) {
      if(!(isdigit(loc[i]))) strncpy(loc, "AA00AA", 7);
    }
  } else {
    strncpy(loc, "AA00AA", 7);
  }

  if(strlen(loc) == 6) {

    for (int i = 4; i <= 5; i++) {
      loc[i] = toupper(loc[i]);
      if((loc[i] < 'A' || loc[i] > 'X')) strncpy(loc, "AA00AA", 7);
    }
  }

  strncpy(locator, loc, 7);

  // Power level validation
  // Only certain increments are allowed
  if(dbm > 60) dbm = 60;

  static const int8_t validDBM[] = {
    -30, -27, -23, -20, -17, -13, -10, -7, -3, 
    0, 3, 7, 10, 13, 17, 20, 23, 27, 30, 33, 37, 40,
    43, 47, 50, 53, 57, 60,
  };

  for(i = 0; i < sizeof(validDBM) / sizeof(validDBM[0]); i++) {
    if(dbm == validDBM[i]) power = dbm;
  }

  // If we got this far, we have an invalid power level, so we'll round down
  for(i = 1; i < sizeof(validDBM) / sizeof(validDBM[0]); i++) {
    if(dbm < validDBM[i] && dbm >= validDBM[i - 1]) power = validDBM[i - 1];
  }
}


#if 0
/*
 * fsq_encode(const char * from_call, const char * message, uint8_t * symbols)
 *
 * Takes an arbitrary message and returns a FSQ channel symbol table.
 *
 * from_call - Callsign of issuing station (maximum size: 20)
 * message - Null-terminated message string, no greater than 130 chars in length
 * symbols - Array of channel symbols to transmit returned by the method.
 *  Ensure that you pass a uint8_t array of at least the size of the message
 *  plus 5 characters to the method. Terminated in 0xFF.
 *
 */
void JTEncode::fsq_encode(const char * from_call, const char * message, uint8_t * symbols)
{
  char tx_buffer[155];
  char * tx_message;
  uint16_t symbol_pos = 0;
  uint8_t i, fch, vcode1, vcode2, tone;
  uint8_t cur_tone = 0;

  // Clear out the transmit buffer
  // -----------------------------
  memset(tx_buffer, 0, 155);

  // Create the message to be transmitted
  // ------------------------------------
  sprintf(tx_buffer, "  \n%s: %s", from_call, message);

  tx_message = tx_buffer;

  // Iterate through the message and encode
  // --------------------------------------
  while(*tx_message != '\0')
  {
    for(i = 0; i < NGLYPHS; i++)
    {
      uint8_t ch = (uint8_t)*tx_message;

      // Check each element of the varicode table to see if we've found the
      // character we're trying to send.
      fch = pgm_read_byte(&fsq_code_table[i].ch);

      if(fch == ch)
      {
          // Found the character, now fetch the varicode chars
          vcode1 = pgm_read_byte(&(fsq_code_table[i].var[0]));
          vcode2 = pgm_read_byte(&(fsq_code_table[i].var[1]));

          // Transmit the appropriate tone per a varicode char
          if(vcode2 == 0)
          {
            // If the 2nd varicode char is a 0 in the table,
            // we are transmitting a lowercase character, and thus
            // only transmit one tone for this character.

            // Generate tone
            cur_tone = ((cur_tone + vcode1 + 1) % 33);
            symbols[symbol_pos++] = cur_tone;
          }
          else
          {
            // If the 2nd varicode char is anything other than 0 in
            // the table, then we need to transmit both

            // Generate 1st tone
            cur_tone = ((cur_tone + vcode1 + 1) % 33);
            symbols[symbol_pos++] = cur_tone;

            // Generate 2nd tone
            cur_tone = ((cur_tone + vcode2 + 1) % 33);
            symbols[symbol_pos++] = cur_tone;
          }
          break; // We've found and transmitted the char,
             // so exit the for loop
        }
    }

    tx_message++;
  }

  // Message termination
  // ----------------
  symbols[symbol_pos] = 0xff;
}

/*
 * fsq_dir_encode(const char * from_call, const char * to_call, const char cmd, const char * message, uint8_t * symbols)
 *
 * Takes an arbitrary message and returns a FSQ channel symbol table.
 *
 * from_call - Callsign from which message is directed (maximum size: 20)
 * to_call - Callsign to which message is directed (maximum size: 20)
 * cmd - Directed command
 * message - Null-terminated message string, no greater than 100 chars in length
 * symbols - Array of channel symbols to transmit returned by the method.
 *  Ensure that you pass a uint8_t array of at least the size of the message
 *  plus 5 characters to the method. Terminated in 0xFF.
 *
 */
void JTEncode::fsq_dir_encode(const char * from_call, const char * to_call, const char cmd, const char * message, uint8_t * symbols)
{
  char tx_buffer[155];
  char * tx_message;
  uint16_t symbol_pos = 0;
  uint8_t i, fch, vcode1, vcode2, tone, from_call_crc;
  uint8_t cur_tone = 0;

  // Generate a CRC on from_call
  // ---------------------------
  from_call_crc = crc8(from_call);

  // Clear out the transmit buffer
  // -----------------------------
  memset(tx_buffer, 0, 155);

  // Create the message to be transmitted
  // We are building a directed message here.
  // FSQ very specifically needs "  \b  " in
  // directed mode to indicate EOT. A single backspace won't do it.
  sprintf(tx_buffer, "  \n%s:%02x%s%c%s%s", from_call, from_call_crc, to_call, cmd, message, "  \b  ");

  tx_message = tx_buffer;

  // Iterate through the message and encode
  // --------------------------------------
  while(*tx_message != '\0')
  {
    for(i = 0; i < NGLYPHS; i++)
    {
      uint8_t ch = (uint8_t)*tx_message;

      // Check each element of the varicode table to see if we've found the
      // character we're trying to send.
      fch = pgm_read_byte(&fsq_code_table[i].ch);

      if(fch == ch)
      {
          // Found the character, now fetch the varicode chars
          vcode1 = pgm_read_byte(&(fsq_code_table[i].var[0]));
          vcode2 = pgm_read_byte(&(fsq_code_table[i].var[1]));

          // Transmit the appropriate tone per a varicode char
          if(vcode2 == 0)
          {
            // If the 2nd varicode char is a 0 in the table,
            // we are transmitting a lowercase character, and thus
            // only transmit one tone for this character.

            // Generate tone
            cur_tone = ((cur_tone + vcode1 + 1) % 33);
            symbols[symbol_pos++] = cur_tone;
          }
          else
          {
            // If the 2nd varicode char is anything other than 0 in
            // the table, then we need to transmit both

            // Generate 1st tone
            cur_tone = ((cur_tone + vcode1 + 1) % 33);
            symbols[symbol_pos++] = cur_tone;

            // Generate 2nd tone
            cur_tone = ((cur_tone + vcode2 + 1) % 33);
            symbols[symbol_pos++] = cur_tone;
          }
          break; // We've found and transmitted the char,
             // so exit the for loop
        }
    }

    tx_message++;
  }

  // Message termination
  // ----------------
  symbols[symbol_pos] = 0xff;
}

/*
 * ft8_encode(const char * message, uint8_t * symbols)
 *
 * Takes an arbitrary message of up to 13 allowable characters or a telemetry message
 * of up to 18 hexadecimal digit (in string format) and returns a channel symbol table.
 * Encoded for the FT8 protocol used in WSJT-X v2.0 and beyond (79 channel symbols).
 *
 * message - Type 0.0 free text message or Type 0.5 telemetry message.
 * symbols - Array of channel symbols to transmit returned by the method.
 *  Ensure that you pass a uint8_t array of at least size FT8_SYMBOL_COUNT to the method.
 *
 */
void JTEncode::ft8_encode(const char * msg, uint8_t * symbols)
{
  uint8_t i;

  char message[19];
  memset(message, 0, 19);
  strcpy(message, msg);

  // Bit packing
  // -----------
  uint8_t c[77];
  memset(c, 0, 77);
  ft8_bit_packing(message, c);

  // Message Encoding
  // ----------------
  uint8_t s[FT8_BIT_COUNT];
  ft8_encode(c, s);

  // Merge with sync vector
  // ----------------------
  ft8_merge_sync_vector(s, symbols);
}


/*
 * latlon_to_grid(float lat, float lon, char* ret_grid)
 *
 * Takes a station latitude and longitude provided in decimal degrees format and
 * returns a string with the 6-digit Maidenhead grid designator.
 *
 * lat - Latitude in decimal degrees format.
 * lon - Longitude in decimal degrees format.
 * ret_grid - Derived Maidenhead grid square. A pointer to a character array of
 *   at least 7 bytes must be provided here for the function return value.
 *
 */
static void latlon_to_grid(float lat, float lon, char* ret_grid) {
  char grid[7];
  memset(grid, 0, 7);

  // Bounds checks
  if(lat < -90.0) {
    lat = -90.0;
  }
  if(lat > 90.0) {
    lat = 90.0;
  }
  if(lon < -180.0) {
    lon = -180.0;
  }
  if(lon > 180.0) {
    lon = 180.0;
  }

  // Normalize lat and lon
  lon += 180.0;
  lat += 90.0;

  // Derive first coordinate pair
  grid[0] = (char)((uint8_t)(lon / 20) + 'A');
  grid[1] = (char)((uint8_t)(lat / 10) + 'A');

  // Derive second coordinate pair
  lon = lon - ((uint8_t)(lon / 20) * 20);
  lat = lat - ((uint8_t)(lat / 10) * 10);
  grid[2] = (char)((uint8_t)(lon / 2) + '0');
  grid[3] = (char)((uint8_t)(lat) + '0');

  // Derive third coordinate pair
  lon = lon - ((uint8_t)(lon / 2) * 2);
  lat = lat - ((uint8_t)(lat));
  grid[4] = (char)((uint8_t)(lon * 12) + 'a');
  grid[5] = (char)((uint8_t)(lat * 24) + 'a');

  strncpy(ret_grid, grid, 6);
}


static uint8_t jtCode(char c) {
  // Validate the input then return the proper integer code. Return
  // 255 as an error code if the char is not allowed.
  switch (c) {
  case '0' ... '9':		// Note use of GCC C++ extension
    return static_cast<uint8_t>(c-48);

  case 'A' ... 'Z':		// Note use of GCC C++ extension
    return static_cast<uint8_t>(c-55);

  case ' ': return 36;
  case '+': return 37;
  case '-': return 38;
  case '.': return 39;
  case '/': return 40;
  case '?': return 41;
  default: return 255;
  }
}


uint8_t JTEncode::ft_code(char c)
{
	/* Validate the input then return the proper integer code */
	// Return 255 as an error code if the char is not allowed

	if(isdigit(c))
	{
		return (uint8_t)(c) - 47;
	}
	else if(c >= 'A' && c <= 'Z')
	{
		return (uint8_t)(c) - 54;
	}
	else if(c == ' ')
	{
		return 0;
	}
  else if(c == '+')
	{
		return 37;
	}
	else if(c == '-')
	{
		return 38;
	}
	else if(c == '.')
	{
		return 39;
	}
	else if(c == '/')
	{
		return 40;
	}
	else if(c == '?')
	{
		return 41;
	}
	else
	{
		return 255;
	}
}

uint8_t JTEncode::wspr_code(char c)
{
  // Validate the input then return the proper integer code.
  // Change character to a space if the char is not allowed.

  if(isdigit(c))
	{
		return (uint8_t)(c - 48);
	}
	else if(c == ' ')
	{
		return 36;
	}
	else if(c >= 'A' && c <= 'Z')
	{
		return (uint8_t)(c - 55);
	}
	else
	{
		return 36;
	}
}

int8_t JTEncode::hex2int(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return -1;
}

void JTEncode::jt_message_prep(char * message)
{
  uint8_t i;

  // Pad the message with trailing spaces
  uint8_t len = strlen(message);
  
  for(i = len; i < 13; i++)
  {
    message[i] = ' ';
  }

  // Convert all chars to uppercase
  for(i = 0; i < 13; i++)
  {
    if(islower(message[i]))
    {
      message[i] = toupper(message[i]);
    }
  }
}

void JTEncode::ft_message_prep(char * message)
{
  uint8_t i;
  char temp_msg[14];

  snprintf(temp_msg, 14, "%13s", message);

  // Convert all chars to uppercase
  for(i = 0; i < 13; i++)
  {
    if(islower(temp_msg[i]))
    {
      temp_msg[i] = toupper(temp_msg[i]);
    }
  }

  strcpy(message, temp_msg);
}


void JTEncode::jt65_bit_packing(char * message, uint8_t * c)
{
  uint32_t n1, n2, n3;

  // Find the N values
  n1 = jtCode(message[0]);
  n1 = n1 * 42 + jtCode(message[1]);
  n1 = n1 * 42 + jtCode(message[2]);
  n1 = n1 * 42 + jtCode(message[3]);
  n1 = n1 * 42 + jtCode(message[4]);

  n2 = jtCode(message[5]);
  n2 = n2 * 42 + jtCode(message[6]);
  n2 = n2 * 42 + jtCode(message[7]);
  n2 = n2 * 42 + jtCode(message[8]);
  n2 = n2 * 42 + jtCode(message[9]);

  n3 = jtCode(message[10]);
  n3 = n3 * 42 + jtCode(message[11]);
  n3 = n3 * 42 + jtCode(message[12]);

  // Pack bits 15 and 16 of N3 into N1 and N2,
  // then mask reset of N3 bits
  n1 = (n1 << 1) + ((n3 >> 15) & 1);
  n2 = (n2 << 1) + ((n3 >> 16) & 1);
  n3 = n3 & 0x7fff;

  // Set the freeform message flag
  n3 += 32768;

  c[0] = (n1 >> 22) & 0x003f;
  c[1] = (n1 >> 16) & 0x003f;
  c[2] = (n1 >> 10) & 0x003f;
  c[3] = (n1 >> 4) & 0x003f;
  c[4] = ((n1 & 0x000f) << 2) + ((n2 >> 26) & 0x0003);
  c[5] = (n2 >> 20) & 0x003f;
  c[6] = (n2 >> 14) & 0x003f;
  c[7] = (n2 >> 8) & 0x003f;
  c[8] = (n2 >> 2) & 0x003f;
  c[9] = ((n2 & 0x0003) << 4) + ((n3 >> 12) & 0x000f);
  c[10] = (n3 >> 6) & 0x003f;
  c[11] = n3 & 0x003f;
}

void JTEncode::jt9_bit_packing(char * message, uint8_t * c)
{
  uint32_t n1, n2, n3;

  // Find the N values
  n1 = jtCode(message[0]);
  n1 = n1 * 42 + jtCode(message[1]);
  n1 = n1 * 42 + jtCode(message[2]);
  n1 = n1 * 42 + jtCode(message[3]);
  n1 = n1 * 42 + jtCode(message[4]);

  n2 = jtCode(message[5]);
  n2 = n2 * 42 + jtCode(message[6]);
  n2 = n2 * 42 + jtCode(message[7]);
  n2 = n2 * 42 + jtCode(message[8]);
  n2 = n2 * 42 + jtCode(message[9]);

  n3 = jtCode(message[10]);
  n3 = n3 * 42 + jtCode(message[11]);
  n3 = n3 * 42 + jtCode(message[12]);

  // Pack bits 15 and 16 of N3 into N1 and N2,
  // then mask reset of N3 bits
  n1 = (n1 << 1) + ((n3 >> 15) & 1);
  n2 = (n2 << 1) + ((n3 >> 16) & 1);
  n3 = n3 & 0x7fff;

  // Set the freeform message flag
  n3 += 32768;

  // 71 message bits to pack, plus 1 bit flag for freeform message.
  // 31 zero bits appended to end.
  // N1 and N2 are 28 bits each, N3 is 16 bits
  // A little less work to start with the least-significant bits
  c[3] = (uint8_t)((n1 & 0x0f) << 4);
  n1 = n1 >> 4;
  c[2] = (uint8_t)(n1 & 0xff);
  n1 = n1 >> 8;
  c[1] = (uint8_t)(n1 & 0xff);
  n1 = n1 >> 8;
  c[0] = (uint8_t)(n1 & 0xff);

  c[6] = (uint8_t)(n2 & 0xff);
  n2 = n2 >> 8;
  c[5] = (uint8_t)(n2 & 0xff);
  n2 = n2 >> 8;
  c[4] = (uint8_t)(n2 & 0xff);
  n2 = n2 >> 8;
  c[3] |= (uint8_t)(n2 & 0x0f);

  c[8] = (uint8_t)(n3 & 0xff);
  n3 = n3 >> 8;
  c[7] = (uint8_t)(n3 & 0xff);

  c[9] = 0;
  c[10] = 0;
  c[11] = 0;
  c[12] = 0;
}
#endif

void JTEncode::wspr_bit_packing(uint8_t * c)
{
  uint32_t n, m;

  // Determine if type 1, 2 or 3 message
	char* slash_avail = strchr(callsign, (int)'/');
	if(callsign[0] == '<')
	{
		// Type 3 message
		char base_call[13];
    memset(base_call, 0, 13);
		uint32_t init_val = 146;
		char* bracket_avail = strchr(callsign, (int)'>');
		int call_len = bracket_avail - callsign - 1;
		strncpy(base_call, callsign + 1, call_len);
		uint32_t hash = nhash_(base_call, &call_len, &init_val);
		hash &= 32767;

		// Convert 6 char grid square to "callsign" format for transmission
		// by putting the first character at the end
		char temp_loc = locator[0];
		locator[0] = locator[1];
		locator[1] = locator[2];
		locator[2] = locator[3];
		locator[3] = locator[4];
		locator[4] = locator[5];
		locator[5] = temp_loc;

		n = wspr_code(locator[0]);
		n = n * 36 + wspr_code(locator[1]);
		n = n * 10 + wspr_code(locator[2]);
		n = n * 27 + (wspr_code(locator[3]) - 10);
		n = n * 27 + (wspr_code(locator[4]) - 10);
		n = n * 27 + (wspr_code(locator[5]) - 10);

		m = (hash * 128) - (power + 1) + 64;
	}
	else if(slash_avail == (void *)0)
	{
		// Type 1 message
		pad_callsign(callsign);
		n = wspr_code(callsign[0]);
		n = n * 36 + wspr_code(callsign[1]);
		n = n * 10 + wspr_code(callsign[2]);
		n = n * 27 + (wspr_code(callsign[3]) - 10);
		n = n * 27 + (wspr_code(callsign[4]) - 10);
		n = n * 27 + (wspr_code(callsign[5]) - 10);
		
		m = ((179 - 10 * (locator[0] - 'A') - (locator[2] - '0')) * 180) + 
			(10 * (locator[1] - 'A')) + (locator[3] - '0');
		m = (m * 128) + power + 64;
	}
	else if(slash_avail)
	{
		// Type 2 message
		int slash_pos = slash_avail - callsign;
    uint8_t i;

		// Determine prefix or suffix
		if(callsign[slash_pos + 2] == ' ' || callsign[slash_pos + 2] == 0)
		{
			// Single character suffix
			char base_call[7];
      memset(base_call, 0, 7);
			strncpy(base_call, callsign, slash_pos);
			for(i = 0; i < 7; i++)
			{
				base_call[i] = toupper(base_call[i]);
				if(!(isdigit(base_call[i]) || isupper(base_call[i])))
				{
					base_call[i] = ' ';
				}
			}
			pad_callsign(base_call);

			n = wspr_code(base_call[0]);
			n = n * 36 + wspr_code(base_call[1]);
			n = n * 10 + wspr_code(base_call[2]);
			n = n * 27 + (wspr_code(base_call[3]) - 10);
			n = n * 27 + (wspr_code(base_call[4]) - 10);
			n = n * 27 + (wspr_code(base_call[5]) - 10);

			char x = callsign[slash_pos + 1];
			if(x >= 48 && x <= 57)
			{
				x -= 48;
			}
			else if(x >= 65 && x <= 90)
			{
				x -= 55;
			}
			else
			{
				x = 38;
			}

			m = 60000 - 32768 + x;

			m = (m * 128) + power + 2 + 64;
		}
		else if(callsign[slash_pos + 3] == ' ' || callsign[slash_pos + 3] == 0)
		{
			// Two-digit numerical suffix
			char base_call[7];
      memset(base_call, 0, 7);
			strncpy(base_call, callsign, slash_pos);
			for(i = 0; i < 6; i++)
			{
				base_call[i] = toupper(base_call[i]);
				if(!(isdigit(base_call[i]) || isupper(base_call[i])))
				{
					base_call[i] = ' ';
				}
			}
			pad_callsign(base_call);

			n = wspr_code(base_call[0]);
			n = n * 36 + wspr_code(base_call[1]);
			n = n * 10 + wspr_code(base_call[2]);
			n = n * 27 + (wspr_code(base_call[3]) - 10);
			n = n * 27 + (wspr_code(base_call[4]) - 10);
			n = n * 27 + (wspr_code(base_call[5]) - 10);

			// TODO: needs validation of digit
			m = 10 * (callsign[slash_pos + 1] - 48) + callsign[slash_pos + 2] - 48;
			m = 60000 + 26 + m;
			m = (m * 128) + power + 2 + 64;
		}
		else
		{
			// Prefix
			char prefix[4];
			char base_call[7];
      memset(prefix, 0, 4);
      memset(base_call, 0, 7);
			strncpy(prefix, callsign, slash_pos);
			strncpy(base_call, callsign + slash_pos + 1, 7);

			if(prefix[2] == ' ' || prefix[2] == 0)
			{
				// Right align prefix
				prefix[3] = 0;
				prefix[2] = prefix[1];
				prefix[1] = prefix[0];
				prefix[0] = ' ';
			}

			for(uint8_t i = 0; i < 6; i++)
			{
				base_call[i] = toupper(base_call[i]);
				if(!(isdigit(base_call[i]) || isupper(base_call[i])))
				{
					base_call[i] = ' ';
				}
			}
			pad_callsign(base_call);

			n = wspr_code(base_call[0]);
			n = n * 36 + wspr_code(base_call[1]);
			n = n * 10 + wspr_code(base_call[2]);
			n = n * 27 + (wspr_code(base_call[3]) - 10);
			n = n * 27 + (wspr_code(base_call[4]) - 10);
			n = n * 27 + (wspr_code(base_call[5]) - 10);

			m = 0;
			for(uint8_t i = 0; i < 3; ++i)
			{
				m = 37 * m + wspr_code(prefix[i]);
			}

			if(m >= 32768)
			{
				m -= 32768;
				m = (m * 128) + power + 2 + 64;
			}
			else
			{
				m = (m * 128) + power + 1 + 64;
			}
		}
	}

  // Callsign is 28 bits, locator/power is 22 bits.
	// A little less work to start with the least-significant bits
	c[3] = (uint8_t)((n & 0x0f) << 4);
	n = n >> 4;
	c[2] = (uint8_t)(n & 0xff);
	n = n >> 8;
	c[1] = (uint8_t)(n & 0xff);
	n = n >> 8;
	c[0] = (uint8_t)(n & 0xff);

	c[6] = (uint8_t)((m & 0x03) << 6);
	m = m >> 2;
	c[5] = (uint8_t)(m & 0xff);
	m = m >> 8;
	c[4] = (uint8_t)(m & 0xff);
	m = m >> 8;
	c[3] |= (uint8_t)(m & 0x0f);
	c[7] = 0;
	c[8] = 0;
	c[9] = 0;
	c[10] = 0;
}

void JTEncode::ft8_bit_packing(char* message, uint8_t* codeword)
{
    // Just encoding type 0 free text and type 0.5 telemetry for now

	// The bit packing algorithm is:
	// sum(message(pos) * 42^pos)

	uint8_t i3 = 0;
	uint8_t n3 = 0;
	uint8_t qa[10];
	uint8_t qb[10];
	char c18[19];
	bool telem = false;
	char temp_msg[19];
	memset(qa, 0, 10);
	memset(qb, 0, 10);

	uint8_t i, j, x, i0;
	uint32_t ireg = 0;

	// See if this is a telemetry message
	// Has to be hex digits, can be no more than 18
	for(i = 0; i < 19; ++i)
	{
		if(message[i] == 0 || message[i] == ' ')
		{
			break;
		}
		else if(hex2int(message[i]) == -1)
		{
			telem = false;
			break;
		}
		else
		{
			c18[i] = message[i];
			telem = true;
		}
	}

	// If telemetry
	if(telem)
	{
		// Get the first 18 hex digits
		for(i = 0; i < strlen(message); ++i)
		{
			i0 = i;
			if(message[i] == ' ')
			{
				--i0;
				break;
			}
		}

		memset(c18, 0, 19);
		memmove(c18, message, i0 + 1);
		snprintf(temp_msg, 19, "%*s", 18, c18);

		// Convert all chars to uppercase
	    for(i = 0; i < strlen(temp_msg); i++)
	    {
	      if(islower(temp_msg[i]))
	      {
	        temp_msg[i] = toupper(temp_msg[i]);
	      }
	    }
		strcpy(message, temp_msg);


		uint8_t temp_int;
		temp_int = message[0] == ' ' ? 0 : hex2int(message[0]);
		for(i = 1; i < 4; ++i)
		{
			codeword[i - 1] = (((temp_int << i) & 0x8) >> 3) & 1;
		}
		temp_int = message[1] == ' ' ? 0 : hex2int(message[1]);
		for(i = 0; i < 4; ++i)
		{
			codeword[i + 3] = (((temp_int << i) & 0x8) >> 3) & 1;
		}
		for(i = 0; i < 8; ++i)
		{
			if(message[2 * i + 2] == ' ')
			{
				temp_int = 0;
			}
			else
			{
				temp_int = hex2int(message[2 * i + 2]);
			}
			for(j = 0; j < 4; ++j)
			{
				codeword[(i + 1) * 8 + j - 1] = (((temp_int << j) & 0x8) >> 3) & 1;
			}
			if(message[2 * i + 3] == ' ')
			{
				temp_int = 0;
			}
			else
			{
				temp_int = hex2int(message[2 * i + 3]);
			}
			for(j = 0; j < 4; ++j)
			{
				codeword[(i + 1) * 8 + j + 3] = (((temp_int << j) & 0x8) >> 3) & 1;
			}
		}

		i3 = 0;
		n3 = 5;
	}
	else
	{
		ft_message_prep(message);

		for(i = 0; i < 13; ++i)
		{
			x = ft_code(message[i]);

			// mult
			ireg = 0;
			for(j = 0; j < 9; ++j)
			{
				ireg = (uint8_t)qa[j] * 42 + (uint8_t)((ireg >> 8) & 0xff);
				qb[j] = (uint8_t)(ireg & 0xff);
			}
			qb[9] = (uint8_t)((ireg >> 8) & 0xff);

			// add
			ireg = x << 8;
			for(j = 0; j < 9; ++j)
			{
				ireg = (uint8_t)qb[j] + (uint8_t)((ireg >> 8) & 0xff);
				qa[j] = (uint8_t)(ireg & 0xff);
			}
			qa[9] = (uint8_t)((ireg >> 8) & 0xff);
		}

		// Format bits to output array
		for(i = 1; i < 8; ++i)
		{
			codeword[i - 1] = (((qa[8] << i) & 0x80) >> 7) & 1;
		}
		for(i = 0; i < 8; ++i)
		{
			for(j = 0; j < 8; ++j)
			{
				codeword[(i + 1) * 8 + j - 1] = (((qa[7 - i] << j) & 0x80) >> 7) & 1;
			}
		}
	}

	// Write the message type bits at the end of the array
	for(i = 0; i < 3; ++i)
	{
		codeword[i + 71] = (n3 >> i) & 1;
	}
	for(i = 0; i < 3; ++i)
	{
		codeword[i + 74] = (i3 >> i) & 1;
	}
}

void JTEncode::jt65_interleave(uint8_t * s)
{
  uint8_t i, j;
  uint8_t d[JT65_ENCODE_COUNT];

  // Interleave
  for(i = 0; i < 9; i++)
  {
    for(j = 0; j < 7; j++)
    {
      d[(j * 9) + i] = s[(i * 7) + j];
    }
  }

  memcpy(s, d, JT65_ENCODE_COUNT);
}

void JTEncode::jt9_interleave(uint8_t * s)
{
  uint8_t i, j;
  uint8_t d[JT9_BIT_COUNT];

  // Do the interleave
  for(i = 0; i < JT9_BIT_COUNT; i++)
  {
    //#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__) || defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega16U4__)
    #if defined(__arm__)
    d[jt9i[i]] = s[i];
    #else
    j = pgm_read_byte(&jt9i[i]);
    d[j] = s[i];
    #endif
  }

  memcpy(s, d, JT9_BIT_COUNT);
}

void JTEncode::wspr_interleave(uint8_t * s)
{
  uint8_t d[WSPR_BIT_COUNT];
	uint8_t rev, index_temp, i, j, k;

	i = 0;

	for(j = 0; j < 255; j++)
	{
		// Bit reverse the index
		index_temp = j;
		rev = 0;

		for(k = 0; k < 8; k++)
		{
			if(index_temp & 0x01)
			{
				rev = rev | (1 << (7 - k));
			}
			index_temp = index_temp >> 1;
		}

		if(rev < WSPR_BIT_COUNT)
		{
			d[rev] = s[i];
			i++;
		}

		if(i >= WSPR_BIT_COUNT)
		{
			break;
		}
	}

  memcpy(s, d, WSPR_BIT_COUNT);
}

void JTEncode::jt9_packbits(uint8_t * d, uint8_t * a)
{
  uint8_t i, k;
  k = 0;
  memset(a, 0, JT9_ENCODE_COUNT);

  for(i = 0; i < JT9_ENCODE_COUNT; i++)
  {
    a[i] = (d[k] & 1) << 2;
    k++;

    a[i] |= ((d[k] & 1) << 1);
    k++;

    a[i] |= (d[k] & 1);
    k++;
  }
}


static const uint8_t generator_bits[83][12] = {
    {0b10000011, 0b00101001, 0b11001110, 0b00010001, 0b10111111, 0b00110001, 0b11101010, 0b11110101, 0b00001001, 0b11110010, 0b01111111, 0b11000000},
    {0b01110110, 0b00011100, 0b00100110, 0b01001110, 0b00100101, 0b11000010, 0b01011001, 0b00110011, 0b01010100, 0b10010011, 0b00010011, 0b00100000},
    {0b11011100, 0b00100110, 0b01011001, 0b00000010, 0b11111011, 0b00100111, 0b01111100, 0b01100100, 0b00010000, 0b10100001, 0b10111101, 0b11000000},
    {0b00011011, 0b00111111, 0b01000001, 0b01111000, 0b01011000, 0b11001101, 0b00101101, 0b11010011, 0b00111110, 0b11000111, 0b11110110, 0b00100000},
    {0b00001001, 0b11111101, 0b10100100, 0b11111110, 0b11100000, 0b01000001, 0b10010101, 0b11111101, 0b00000011, 0b01000111, 0b10000011, 0b10100000},
    {0b00000111, 0b01111100, 0b11001100, 0b11000001, 0b00011011, 0b10001000, 0b01110011, 0b11101101, 0b01011100, 0b00111101, 0b01001000, 0b10100000},
    {0b00101001, 0b10110110, 0b00101010, 0b11111110, 0b00111100, 0b10100000, 0b00110110, 0b11110100, 0b11111110, 0b00011010, 0b10011101, 0b10100000},
    {0b01100000, 0b01010100, 0b11111010, 0b11110101, 0b11110011, 0b01011101, 0b10010110, 0b11010011, 0b10110000, 0b11001000, 0b11000011, 0b11100000},
    {0b11100010, 0b00000111, 0b10011000, 0b11100100, 0b00110001, 0b00001110, 0b11101101, 0b00100111, 0b10001000, 0b01001010, 0b11101001, 0b00000000},
    {0b01110111, 0b01011100, 0b10011100, 0b00001000, 0b11101000, 0b00001110, 0b00100110, 0b11011101, 0b10101110, 0b01010110, 0b00110001, 0b10000000},
    {0b10110000, 0b10111000, 0b00010001, 0b00000010, 0b10001100, 0b00101011, 0b11111001, 0b10010111, 0b00100001, 0b00110100, 0b10000111, 0b11000000},
    {0b00011000, 0b10100000, 0b11001001, 0b00100011, 0b00011111, 0b11000110, 0b00001010, 0b11011111, 0b01011100, 0b01011110, 0b10100011, 0b00100000},
    {0b01110110, 0b01000111, 0b00011110, 0b10000011, 0b00000010, 0b10100000, 0b01110010, 0b00011110, 0b00000001, 0b10110001, 0b00101011, 0b10000000},
    {0b11111111, 0b10111100, 0b11001011, 0b10000000, 0b11001010, 0b10000011, 0b01000001, 0b11111010, 0b11111011, 0b01000111, 0b10110010, 0b11100000},
    {0b01100110, 0b10100111, 0b00101010, 0b00010101, 0b10001111, 0b10010011, 0b00100101, 0b10100010, 0b10111111, 0b01100111, 0b00010111, 0b00000000},
    {0b11000100, 0b00100100, 0b00110110, 0b10001001, 0b11111110, 0b10000101, 0b10110001, 0b11000101, 0b00010011, 0b01100011, 0b10100001, 0b10000000},
    {0b00001101, 0b11111111, 0b01110011, 0b10010100, 0b00010100, 0b11010001, 0b10100001, 0b10110011, 0b01001011, 0b00011100, 0b00100111, 0b00000000},
    {0b00010101, 0b10110100, 0b10001000, 0b00110000, 0b01100011, 0b01101100, 0b10001011, 0b10011001, 0b10001001, 0b01001001, 0b01110010, 0b11100000},
    {0b00101001, 0b10101000, 0b10011100, 0b00001101, 0b00111101, 0b11101000, 0b00011101, 0b01100110, 0b01010100, 0b10001001, 0b10110000, 0b11100000},
    {0b01001111, 0b00010010, 0b01101111, 0b00110111, 0b11111010, 0b01010001, 0b11001011, 0b11100110, 0b00011011, 0b11010110, 0b10111001, 0b01000000},
    {0b10011001, 0b11000100, 0b01110010, 0b00111001, 0b11010000, 0b11011001, 0b01111101, 0b00111100, 0b10000100, 0b11100000, 0b10010100, 0b00000000},
    {0b00011001, 0b00011001, 0b10110111, 0b01010001, 0b00011001, 0b01110110, 0b01010110, 0b00100001, 0b10111011, 0b01001111, 0b00011110, 0b10000000},
    {0b00001001, 0b11011011, 0b00010010, 0b11010111, 0b00110001, 0b11111010, 0b11101110, 0b00001011, 0b10000110, 0b11011111, 0b01101011, 0b10000000},
    {0b01001000, 0b10001111, 0b11000011, 0b00111101, 0b11110100, 0b00111111, 0b10111101, 0b11101110, 0b10100100, 0b11101010, 0b11111011, 0b01000000},
    {0b10000010, 0b01110100, 0b00100011, 0b11101110, 0b01000000, 0b10110110, 0b01110101, 0b11110111, 0b01010110, 0b11101011, 0b01011111, 0b11100000},
    {0b10101011, 0b11100001, 0b10010111, 0b11000100, 0b10000100, 0b11001011, 0b01110100, 0b01110101, 0b01110001, 0b01000100, 0b10101001, 0b10100000},
    {0b00101011, 0b01010000, 0b00001110, 0b01001011, 0b11000000, 0b11101100, 0b01011010, 0b01101101, 0b00101011, 0b11011011, 0b11011101, 0b00000000},
    {0b11000100, 0b01110100, 0b10101010, 0b01010011, 0b11010111, 0b00000010, 0b00011000, 0b01110110, 0b00010110, 0b01101001, 0b00110110, 0b00000000},
    {0b10001110, 0b10111010, 0b00011010, 0b00010011, 0b11011011, 0b00110011, 0b10010000, 0b10111101, 0b01100111, 0b00011000, 0b11001110, 0b11000000},
    {0b01110101, 0b00111000, 0b01000100, 0b01100111, 0b00111010, 0b00100111, 0b01111000, 0b00101100, 0b11000100, 0b00100000, 0b00010010, 0b11100000},
    {0b00000110, 0b11111111, 0b10000011, 0b10100001, 0b01000101, 0b11000011, 0b01110000, 0b00110101, 0b10100101, 0b11000001, 0b00100110, 0b10000000},
    {0b00111011, 0b00110111, 0b01000001, 0b01111000, 0b01011000, 0b11001100, 0b00101101, 0b11010011, 0b00111110, 0b11000011, 0b11110110, 0b00100000},
    {0b10011010, 0b01001010, 0b01011010, 0b00101000, 0b11101110, 0b00010111, 0b11001010, 0b10011100, 0b00110010, 0b01001000, 0b01000010, 0b11000000},
    {0b10111100, 0b00101001, 0b11110100, 0b01100101, 0b00110000, 0b10011100, 0b10010111, 0b01111110, 0b10001001, 0b01100001, 0b00001010, 0b01000000},
    {0b00100110, 0b01100011, 0b10101110, 0b01101101, 0b11011111, 0b10001011, 0b01011100, 0b11100010, 0b10111011, 0b00101001, 0b01001000, 0b10000000},
    {0b01000110, 0b11110010, 0b00110001, 0b11101111, 0b11100100, 0b01010111, 0b00000011, 0b01001100, 0b00011000, 0b00010100, 0b01000001, 0b10000000},
    {0b00111111, 0b10110010, 0b11001110, 0b10000101, 0b10101011, 0b11101001, 0b10110000, 0b11000111, 0b00101110, 0b00000110, 0b11111011, 0b11100000},
    {0b11011110, 0b10000111, 0b01001000, 0b00011111, 0b00101000, 0b00101100, 0b00010101, 0b00111001, 0b01110001, 0b10100000, 0b10100010, 0b11100000},
    {0b11111100, 0b11010111, 0b11001100, 0b11110010, 0b00111100, 0b01101001, 0b11111010, 0b10011001, 0b10111011, 0b10100001, 0b01000001, 0b00100000},
    {0b11110000, 0b00100110, 0b00010100, 0b01000111, 0b11101001, 0b01001001, 0b00001100, 0b10101000, 0b11100100, 0b01110100, 0b11001110, 0b11000000},
    {0b01000100, 0b00010000, 0b00010001, 0b01011000, 0b00011000, 0b00011001, 0b01101111, 0b10010101, 0b11001101, 0b11010111, 0b00000001, 0b00100000},
    {0b00001000, 0b10001111, 0b11000011, 0b00011101, 0b11110100, 0b10111111, 0b10111101, 0b11100010, 0b10100100, 0b11101010, 0b11111011, 0b01000000},
    {0b10111000, 0b11111110, 0b11110001, 0b10110110, 0b00110000, 0b01110111, 0b00101001, 0b11111011, 0b00001010, 0b00000111, 0b10001100, 0b00000000},
    {0b01011010, 0b11111110, 0b10100111, 0b10101100, 0b11001100, 0b10110111, 0b01111011, 0b10111100, 0b10011101, 0b10011001, 0b10101001, 0b00000000},
    {0b01001001, 0b10100111, 0b00000001, 0b01101010, 0b11000110, 0b01010011, 0b11110110, 0b01011110, 0b11001101, 0b11001001, 0b00000111, 0b01100000},
    {0b00011001, 0b01000100, 0b11010000, 0b10000101, 0b10111110, 0b01001110, 0b01111101, 0b10101000, 0b11010110, 0b11001100, 0b01111101, 0b00000000},
    {0b00100101, 0b00011111, 0b01100010, 0b10101101, 0b11000100, 0b00000011, 0b00101111, 0b00001110, 0b11100111, 0b00010100, 0b00000000, 0b00100000},
    {0b01010110, 0b01000111, 0b00011111, 0b10000111, 0b00000010, 0b10100000, 0b01110010, 0b00011110, 0b00000000, 0b10110001, 0b00101011, 0b10000000},
    {0b00101011, 0b10001110, 0b01001001, 0b00100011, 0b11110010, 0b11011101, 0b01010001, 0b11100010, 0b11010101, 0b00110111, 0b11111010, 0b00000000},
    {0b01101011, 0b01010101, 0b00001010, 0b01000000, 0b10100110, 0b01101111, 0b01000111, 0b01010101, 0b11011110, 0b10010101, 0b11000010, 0b01100000},
    {0b10100001, 0b10001010, 0b11010010, 0b10001101, 0b01001110, 0b00100111, 0b11111110, 0b10010010, 0b10100100, 0b11110110, 0b11001000, 0b01000000},
    {0b00010000, 0b11000010, 0b11100101, 0b10000110, 0b00111000, 0b10001100, 0b10111000, 0b00101010, 0b00111101, 0b10000000, 0b01110101, 0b10000000},
    {0b11101111, 0b00110100, 0b10100100, 0b00011000, 0b00010111, 0b11101110, 0b00000010, 0b00010011, 0b00111101, 0b10110010, 0b11101011, 0b00000000},
    {0b01111110, 0b10011100, 0b00001100, 0b01010100, 0b00110010, 0b01011010, 0b10011100, 0b00010101, 0b10000011, 0b01101110, 0b00000000, 0b00000000},
    {0b00110110, 0b10010011, 0b11100101, 0b01110010, 0b11010001, 0b11111101, 0b11100100, 0b11001101, 0b11110000, 0b01111001, 0b11101000, 0b01100000},
    {0b10111111, 0b10110010, 0b11001110, 0b11000101, 0b10101011, 0b11100001, 0b10110000, 0b11000111, 0b00101110, 0b00000111, 0b11111011, 0b11100000},
    {0b01111110, 0b11100001, 0b10000010, 0b00110000, 0b11000101, 0b10000011, 0b11001100, 0b11001100, 0b01010111, 0b11010100, 0b10110000, 0b10000000},
    {0b10100000, 0b01100110, 0b11001011, 0b00101111, 0b11101101, 0b10101111, 0b11001001, 0b11110101, 0b00100110, 0b01100100, 0b00010010, 0b01100000},
    {0b10111011, 0b00100011, 0b01110010, 0b01011010, 0b10111100, 0b01000111, 0b11001100, 0b01011111, 0b01001100, 0b11000100, 0b11001101, 0b00100000},
    {0b11011110, 0b11011001, 0b11011011, 0b10100011, 0b10111110, 0b11100100, 0b00001100, 0b01011001, 0b10110101, 0b01100000, 0b10011011, 0b01000000},
    {0b11011001, 0b10100111, 0b00000001, 0b01101010, 0b11000110, 0b01010011, 0b11100110, 0b11011110, 0b11001101, 0b11001001, 0b00000011, 0b01100000},
    {0b10011010, 0b11010100, 0b01101010, 0b11101101, 0b01011111, 0b01110000, 0b01111111, 0b00101000, 0b00001010, 0b10110101, 0b11111100, 0b01000000},
    {0b11100101, 0b10010010, 0b00011100, 0b01110111, 0b10000010, 0b00100101, 0b10000111, 0b00110001, 0b01101101, 0b01111101, 0b00111100, 0b00100000},
    {0b01001111, 0b00010100, 0b11011010, 0b10000010, 0b01000010, 0b10101000, 0b10111000, 0b01101101, 0b11001010, 0b01110011, 0b00110101, 0b00100000},
    {0b10001011, 0b10001011, 0b01010000, 0b01111010, 0b11010100, 0b01100111, 0b11010100, 0b01000100, 0b00011101, 0b11110111, 0b01110000, 0b11100000},
    {0b00100010, 0b10000011, 0b00011100, 0b10011100, 0b11110001, 0b00010110, 0b10010100, 0b01100111, 0b10101101, 0b00000100, 0b10110110, 0b10000000},
    {0b00100001, 0b00111011, 0b10000011, 0b10001111, 0b11100010, 0b10101110, 0b01010100, 0b11000011, 0b10001110, 0b11100111, 0b00011000, 0b00000000},
    {0b01011101, 0b10010010, 0b01101011, 0b01101101, 0b11010111, 0b00011111, 0b00001000, 0b01010001, 0b10000001, 0b10100100, 0b11100001, 0b00100000},
    {0b01100110, 0b10101011, 0b01111001, 0b11010100, 0b10110010, 0b10011110, 0b11100110, 0b11100110, 0b10010101, 0b00001001, 0b11100101, 0b01100000},
    {0b10010101, 0b10000001, 0b01001000, 0b01101000, 0b00101101, 0b01110100, 0b10001010, 0b00111000, 0b11011101, 0b01101000, 0b10111010, 0b10100000},
    {0b10111000, 0b11001110, 0b00000010, 0b00001100, 0b11110000, 0b01101001, 0b11000011, 0b00101010, 0b01110010, 0b00111010, 0b10110001, 0b01000000},
    {0b11110100, 0b00110011, 0b00011101, 0b01101101, 0b01000110, 0b00010110, 0b00000111, 0b11101001, 0b01010111, 0b01010010, 0b01110100, 0b01100000},
    {0b01101101, 0b10100010, 0b00111011, 0b10100100, 0b00100100, 0b10111001, 0b01011001, 0b01100001, 0b00110011, 0b11001111, 0b10011100, 0b10000000},
    {0b10100110, 0b00110110, 0b10111100, 0b10111100, 0b01111011, 0b00110000, 0b11000101, 0b11111011, 0b11101010, 0b11100110, 0b01111111, 0b11100000},
    {0b01011100, 0b10110000, 0b11011000, 0b01101010, 0b00000111, 0b11011111, 0b01100101, 0b01001010, 0b10010000, 0b10001001, 0b10100010, 0b00000000},
    {0b11110001, 0b00011111, 0b00010000, 0b01101000, 0b01001000, 0b01111000, 0b00001111, 0b11001001, 0b11101100, 0b11011101, 0b10000000, 0b10100000},
    {0b00011111, 0b10111011, 0b01010011, 0b01100100, 0b11111011, 0b10001101, 0b00101100, 0b10011101, 0b01110011, 0b00001101, 0b01011011, 0b10100000},
    {0b11111100, 0b10111000, 0b01101011, 0b11000111, 0b00001010, 0b01010000, 0b11001001, 0b11010000, 0b00101010, 0b01011101, 0b00000011, 0b01000000},
    {0b10100101, 0b00110100, 0b01000011, 0b00110000, 0b00101001, 0b11101010, 0b11000001, 0b01011111, 0b00110010, 0b00101110, 0b00110100, 0b11000000},
    {0b11001001, 0b10001001, 0b11011001, 0b11000111, 0b11000011, 0b11010011, 0b10111000, 0b11000101, 0b01011101, 0b01110101, 0b00010011, 0b00000000},
    {0b01111011, 0b10110011, 0b10001011, 0b00101111, 0b00000001, 0b10000110, 0b11010100, 0b01100110, 0b01000011, 0b10101110, 0b10010110, 0b00100000},
    {0b00100110, 0b01000100, 0b11101011, 0b10101101, 0b11101011, 0b01000100, 0b10111001, 0b01000110, 0b01111101, 0b00011111, 0b01000010, 0b11000000},
    {0b01100000, 0b10001100, 0b11001000, 0b01010111, 0b01011001, 0b01001011, 0b11111011, 0b10110101, 0b01011101, 0b01101001, 0b01100000, 0b00000000},
};


void JTEncode::ft8_encode(uint8_t* codeword, uint8_t* symbols)
{
	const uint8_t FT8_N = 174;
	const uint8_t FT8_K = 91;
	const uint8_t FT8_M = FT8_N - FT8_K;

	uint8_t tempchar[FT8_K];
	uint8_t message91[FT8_K];
	uint8_t pchecks[FT8_M];
	uint8_t i1_msg_bytes[12];
	uint8_t i, j;
	uint16_t ncrc14;

	crc_t crc;
	crc_cfg_t crc_cfg;
	crc_cfg.reflect_in = 0;
	crc_cfg.xor_in = 0;
	crc_cfg.reflect_out = 0;
	crc_cfg.xor_out = 0;
	crc = crc_init(&crc_cfg);

	// Add 14-bit CRC to form 91-bit message
	memset(tempchar, 0, 91);
	memcpy(tempchar, codeword, 77);
	tempchar[77] = 0;
	tempchar[78] = 0;
	tempchar[79] = 0;
	memset(i1_msg_bytes, 0, 12);
	for(i = 0; i < 10; ++i)
	{
		for(j = 0; j < 8; ++j)
		{
			i1_msg_bytes[i] <<= 1;
			i1_msg_bytes[i] |= tempchar[i * 8 + j];
		}
	}

	ncrc14 = crc_update(&crc_cfg, crc, (unsigned char *)i1_msg_bytes, 12);
	crc = crc_finalize(&crc_cfg, crc);

	for(i = 0; i < 14; ++i)
	{
		if((((ncrc14 << (i + 2)) & 0x8000) >> 15) & 1)
		{
			tempchar[i + 77] = 1;
		}
		else
		{
			tempchar[i + 77] = 0;
		}
	}
	memcpy(message91, tempchar, 91);

	for(i = 0; i < FT8_M; ++i)
	{
		uint32_t nsum = 0;
		for(j = 0; j < FT8_K; ++j)
		{
      #if defined(__arm__)
      uint8_t bits = generator_bits[i][j / 8];
      #else
      uint8_t bits = pgm_read_byte(&(generator_bits[i][j / 8]));
      #endif
			bits <<= (j % 8);
			bits &= 0x80;
			bits >>= 7;
			bits &= 1;
			nsum += (message91[j] * bits);
		}
		pchecks[i] = nsum % 2;
	}

	memcpy(symbols, message91, FT8_K);
	memcpy(symbols + FT8_K, pchecks, FT8_M);
}

void JTEncode::jt65_merge_sync_vector(uint8_t * g, uint8_t * symbols)
{
  uint8_t i, j = 0;
  const uint8_t sync_vector[JT65_SYMBOL_COUNT] =
  {1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0,
   0, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1,
   0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1,
   0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1,
   1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1,
   0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1,
   1, 1, 1, 1, 1, 1};

  for(i = 0; i < JT65_SYMBOL_COUNT; i++)
  {
    if(sync_vector[i])
    {
      symbols[i] = 0;
    }
    else
    {
      symbols[i] = g[j] + 2;
      j++;
    }
  }
}

void JTEncode::jt9_merge_sync_vector(uint8_t * g, uint8_t * symbols)
{
  uint8_t i, j = 0;
  const uint8_t sync_vector[JT9_SYMBOL_COUNT] =
  {1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
   0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1,
   0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 1, 0, 1};

  for(i = 0; i < JT9_SYMBOL_COUNT; i++)
  {
    if(sync_vector[i])
    {
      symbols[i] = 0;
    }
    else
    {
      symbols[i] = g[j] + 1;
      j++;
    }
  }
}

void JTEncode::jt4_merge_sync_vector(uint8_t * g, uint8_t * symbols)
{
  uint8_t i;
  const uint8_t sync_vector[JT4_SYMBOL_COUNT] =
	{0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0,
   0, 0, 0, 0, 1, 1, 0, 0, 0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,1 ,0 ,1 ,1,
   0, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0,
   1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 0,
   0, 1, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0,
   1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1,
   1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1,
   0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1,
   1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1,
   0, 1, 1, 1, 1, 0, 1, 0, 1};

	for(i = 0; i < JT4_SYMBOL_COUNT; i++)
	{
		symbols[i] = sync_vector[i] + (2 * g[i]);
	}
}

void JTEncode::wspr_merge_sync_vector(uint8_t * g, uint8_t * symbols)
{
  uint8_t i;
  const uint8_t sync_vector[WSPR_SYMBOL_COUNT] =
	{1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0,
	 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0,
	 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 1,
	 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0,
	 1, 1, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1,
	 0, 0, 1, 0, 0, 1, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1,
	 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0,
	 1, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0};

	for(i = 0; i < WSPR_SYMBOL_COUNT; i++)
	{
		symbols[i] = sync_vector[i] + (2 * g[i]);
	}
}

void JTEncode::ft8_merge_sync_vector(uint8_t* symbols, uint8_t* output)
{
	const uint8_t costas7x7[7] = {3, 1, 4, 0, 6, 5, 2};
	const uint8_t graymap[8] = {0, 1, 3, 2, 5, 6, 4, 7};
	uint8_t i, j, k, idx;

	// Insert Costas sync arrays
	memcpy(output, costas7x7, 7);
	memcpy(output + 36, costas7x7, 7);
	memcpy(output + FT8_SYMBOL_COUNT - 7, costas7x7, 7);

	k = 6;
	for(j = 0; j < 58; ++j) // 58 data symbols
	{
		i = 3 * j;
		++k;
		if(j == 29)
		{
			k += 7;
		}
		idx = symbols[i] * 4 + symbols[i + 1] * 2 + symbols[i + 2];
		output[k] = graymap[idx];
	}
}

void JTEncode::convolve(uint8_t * c, uint8_t * s, uint8_t message_size, uint8_t bit_size)
{
  uint32_t reg_0 = 0;
  uint32_t reg_1 = 0;
  uint32_t reg_temp = 0;
  uint8_t input_bit, parity_bit;
  uint8_t bit_count = 0;
  uint8_t i, j, k;

  for(i = 0; i < message_size; i++)
  {
    for(j = 0; j < 8; j++)
    {
      // Set input bit according the MSB of current element
      input_bit = (((c[i] << j) & 0x80) == 0x80) ? 1 : 0;

      // Shift both registers and put in the new input bit
      reg_0 = reg_0 << 1;
      reg_1 = reg_1 << 1;
      reg_0 |= (uint32_t)input_bit;
      reg_1 |= (uint32_t)input_bit;

      // AND Register 0 with feedback taps, calculate parity
      reg_temp = reg_0 & 0xf2d05351;
      parity_bit = 0;
      for(k = 0; k < 32; k++)
      {
        parity_bit = parity_bit ^ (reg_temp & 0x01);
        reg_temp = reg_temp >> 1;
      }
      s[bit_count] = parity_bit;
      bit_count++;

      // AND Register 1 with feedback taps, calculate parity
      reg_temp = reg_1 & 0xe4613c47;
      parity_bit = 0;
      for(k = 0; k < 32; k++)
      {
        parity_bit = parity_bit ^ (reg_temp & 0x01);
        reg_temp = reg_temp >> 1;
      }
      s[bit_count] = parity_bit;
      bit_count++;
      if(bit_count >= bit_size)
      {
        break;
      }
    }
  }
}

void JTEncode::rs_encode(uint8_t * data, uint8_t * symbols)
{
  // Adapted from wrapkarn.c in the WSJT-X source code
  uint8_t dat1[12];
  uint8_t b[51];
  uint8_t sym[JT65_ENCODE_COUNT];
  uint8_t i;

  // Reverse data order for the Karn codec.
  for(i = 0; i < 12; i++)
  {
    dat1[i] = data[11 - i];
  }

  // Compute the parity symbols
  encode_rs_int(rs_inst, dat1, b);

  // Move parity symbols and data into symbols array, in reverse order.
  for (i = 0; i < 51; i++)
  {
    sym[50 - i] = b[i];
  }

  for (i = 0; i < 12; i++)
  {
    sym[i + 51] = dat1[11 - i];
  }

  memcpy(symbols, sym, JT65_ENCODE_COUNT);
}

uint8_t JTEncode::crc8(const char * text)
{
  uint8_t crc = '\0';
  uint8_t ch;

  int i;
  for(i = 0; i < strlen(text); i++)
  {
    ch = text[i];
    //#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__) || defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega16U4__)
    #if defined(__arm__)
    crc = crc8_table[(crc) ^ ch];
    #else
    crc = pgm_read_byte(&(crc8_table[(crc) ^ ch]));
    #endif
    crc &= 0xFF;
  }

  return crc;
}

void JTEncode::pad_callsign(char * call)
{
	// If only the 2nd character is a digit, then pad with a space.
	// If this happens, then the callsign will be truncated if it is
	// longer than 6 characters.
	if(isdigit(call[1]) && isupper(call[2]))
	{
		// memmove(call + 1, call, 6);
    call[5] = call[4];
    call[4] = call[3];
    call[3] = call[2];
    call[2] = call[1];
    call[1] = call[0];
		call[0] = ' ';
	}

	// Now the 3rd charcter in the callsign must be a digit
	// if(call[2] < '0' || call[2] > '9')
	// {
	// 	// return 1;
	// }
}


static uint8_t jtCode(char c) {
  // Validate the input then return the proper integer code. Return
  // 255 as an error code if the char is not allowed.
  switch (c) {
  case '0' ... '9':		// Note use of GCC C++ extension
    return static_cast<uint8_t>(c-48);

  case 'A' ... 'Z':		// Note use of GCC C++ extension
    return static_cast<uint8_t>(c-55);

  case ' ': return 36;
  case '+': return 37;
  case '-': return 38;
  case '.': return 39;
  case '/': return 40;
  case '?': return 41;
  default: return 255;
  }
}


static uint8_t ftCode(char c) {
  // Validate the input then return the proper integer code. Return
  // 255 as an error code if the char is not allowed.
  switch (c) {
  case '0' ... '9':		// Note use of GCC C++ extension
    return static_cast<uint8_t>(c-47);

  case 'A' ... 'Z':		// Note use of GCC C++ extension
    return static_cast<uint8_t>(c-54);

  case ' ': return 0;
  case '+': return 37;
  case '-': return 38;
  case '.': return 39;
  case '/': return 40;
  case '?': return 41;
  default: return 255;
  }
}



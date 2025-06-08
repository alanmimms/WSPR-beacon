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
 * modularity. Its target-specific nature has been eliminated to make
 * it more broadly applicable to try to limit rampant forkulation that
 * has been common for this code in the past.
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
#include <nhash.h>

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>


static void convolve(uint8_t * c, uint8_t * s, uint8_t message_size, uint8_t bit_size) {
  uint32_t reg_0 = 0;
  uint32_t reg_1 = 0;
  uint32_t reg_temp = 0;
  uint8_t input_bit, parity_bit;
  uint8_t bit_count = 0;

  for (int i = 0; i < message_size; i++) {

    for (int j = 0; j < 8; j++) {
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

      for (int k = 0; k < 32; k++) {
        parity_bit = parity_bit ^ (reg_temp & 0x01);
        reg_temp = reg_temp >> 1;
      }

      s[bit_count] = parity_bit;
      bit_count++;

      // AND Register 1 with feedback taps, calculate parity
      reg_temp = reg_1 & 0xe4613c47;
      parity_bit = 0;

      for (int k = 0; k < 32; k++) {
        parity_bit = parity_bit ^ (reg_temp & 0x01);
        reg_temp = reg_temp >> 1;
      }

      s[bit_count] = parity_bit;
      bit_count++;
      if (bit_count >= bit_size) break;
    }
  }
}


static void padCallsign(char * call) {
  // If only the 2nd character is a digit, then pad with a space.
  // If this happens, then the callsign will be truncated if it is
  // longer than 6 characters.
  if (isdigit(call[1]) && isupper(call[2])) {
    // memmove(call + 1, call, 6);
    call[5] = call[4];
    call[4] = call[3];
    call[3] = call[2];
    call[2] = call[1];
    call[1] = call[0];
    call[0] = ' ';
  }

  // Now the 3rd charcter in the callsign must be a digit
  // if (call[2] < '0' || call[2] > '9')
  // {
  // 	// return 1;
  // }
}


/*
 * WSPEncoder::encode(const char * call, const char * loc, const uint8_t dbm, uint8_t * symbols)
 *
 * Takes a callsign, grid locator, and power level and returns a WSPR symbol
 * table for a Type 1, 2, or 3 message.
 *
 * call - Callsign (12 characters maximum).
 * loc - Maidenhead grid locator (6 characters maximum).
 * dbm - Output power in dBm.
 * symbols - Array of channel symbols to transmit returned by the method.
 *  Ensure that you pass a uint8_t array of at least size MSGSIZE to the method.
 *
 */

void WSPREncoder::encode(const char *callP, const char *locP, const uint8_t dbm) {
  char call[13];
  char loc[7];

  strcpy(call, callP);
  strcpy(loc, locP);

  // Ensure that the message text conforms to standards
  // --------------------------------------------------
  messagePrep(call, loc, dbm);

  // Bit packing
  // -----------
  uint8_t c[11];
  bitPacking(c);

  // Convolutional Encoding
  // ---------------------
  uint8_t encoded[sizeof(this->txBuf)];
  convolve(c, encoded, 11, sizeof(this->txBuf));

  // Interleaving
  // ------------
  interleave(encoded);

  // Merge with sync vector
  // ----------------------
  mergeSyncVector(encoded);
}


void WSPREncoder::messagePrep(const char *callP, const char *locP, const uint8_t dbm) {
  // Callsign validation and padding
  // -------------------------------
	
  // Ensure that the only allowed characters are digits, uppercase letters, slash, and angle brackets
  for (int i = 0; i < 12; i++) {

    if (callP[i] != '/' && callP[i] != '<' && callP[i] != '>') {
      callsign[i] = toupper(callP[i]);

      if (!(isdigit(callP[i]) || isupper(callP[i]))) {
	callsign[i] = ' ';
      }
    } else {
      callsign[i] = callP[i];
    }
  }

  callsign[12] = 0;

  // Grid locator validation
  if (strlen(locP) == 4 || strlen(locP) == 6) {

    for (int i = 0; i <= 1; i++) {
      locator[i] = toupper(locP[i]);
      if ((locP[i] < 'A' || locP[i] > 'R')) strncpy(locator, "AA00AA", 7);
    }

    for (int i = 2; i <= 3; i++) {
      if (!(isdigit(locP[i]))) strncpy(locator, "AA00AA", 7);
    }
  } else {
    strncpy(locator, "AA00AA", 7);
  }

  if (strlen(locator) == 6) {

    for (int i = 4; i <= 5; i++) {
      locator[i] = toupper(locator[i]);
      if ((locator[i] < 'A' || locator[i] > 'X')) strncpy(locator, "AA00AA", 7);
    }
  }

  // Power level validation
  // Only certain increments are allowed
  if (dbm > 60) dbm = 60;

  static const int8_t validDBM[] = {
    -30, -27, -23, -20, -17, -13, -10, -7, -3, 
    0, 3, 7, 10, 13, 17, 20, 23, 27, 30, 33, 37, 40,
    43, 47, 50, 53, 57, 60,
  };

  for (int i = 0; i < sizeof(validDBM) / sizeof(validDBM[0]); i++) {

    if (dbm == validDBM[i]) {
      power = dbm;
      break;
    }
  }

  // If we got this far, we have an invalid power level, so we'll round down
  for (int i = 1; i < sizeof(validDBM) / sizeof(validDBM[0]); i++) {

    if (dbm < validDBM[i] && dbm >= validDBM[i - 1]) {
      power = validDBM[i - 1];
      break;
    }
  }
}



static inline uint8_t wsprCode(const char c) {
  // Validate the input then return the proper integer code.
  // Change character to a space if the char is not allowed.

  switch (c) {
  case '0' ... '9': return static_cast<uint8_t>(c - 48);
  case 'A' ... 'Z': return static_cast<uint8_t>(c - 55);
  case ' ': return 36;
  default: return 36;
  }
}


void WSPREncoder::bitPacking(uint8_t * c) {
  uint32_t n, m;

  // Determine if type 1, 2 or 3 message
  char* slash_avail = strchr(callsign, '/');

  if (callsign[0] == '<') {
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

    n = wsprCode(locator[0]);
    n = n * 36 + wsprCode(locator[1]);
    n = n * 10 + wsprCode(locator[2]);
    n = n * 27 + (wsprCode(locator[3]) - 10);
    n = n * 27 + (wsprCode(locator[4]) - 10);
    n = n * 27 + (wsprCode(locator[5]) - 10);

    m = (hash * 128) - (power + 1) + 64;
  } else if (slash_avail == (void *)0) {
    // Type 1 message
    padCallsign(callsign);
    n = wsprCode(callsign[0]);
    n = n * 36 + wsprCode(callsign[1]);
    n = n * 10 + wsprCode(callsign[2]);
    n = n * 27 + (wsprCode(callsign[3]) - 10);
    n = n * 27 + (wsprCode(callsign[4]) - 10);
    n = n * 27 + (wsprCode(callsign[5]) - 10);
		
    m = ((179 - 10 * (locator[0] - 'A') - (locator[2] - '0')) * 180) + 
      (10 * (locator[1] - 'A')) + (locator[3] - '0');
    m = (m * 128) + power + 64;
  } else if (slash_avail) {
    // Type 2 message
    int slash_pos = slash_avail - callsign;

    // Determine prefix or suffix
    if (callsign[slash_pos + 2] == ' ' || callsign[slash_pos + 2] == 0) {
      // Single character suffix
      char base_call[7];
      memset(base_call, 0, 7);
      strncpy(base_call, callsign, slash_pos);

      for (int i = 0; i < 7; i++) {
	base_call[i] = toupper(base_call[i]);
	if (!(isdigit(base_call[i]) || isupper(base_call[i]))) base_call[i] = ' ';
      }

      padCallsign(base_call);

      n = wsprCode(base_call[0]);
      n = n * 36 + wsprCode(base_call[1]);
      n = n * 10 + wsprCode(base_call[2]);
      n = n * 27 + (wsprCode(base_call[3]) - 10);
      n = n * 27 + (wsprCode(base_call[4]) - 10);
      n = n * 27 + (wsprCode(base_call[5]) - 10);

      char x = callsign[slash_pos + 1];
      if (x >= 48 && x <= 57) {
	x -= 48;
      } else if (x >= 65 && x <= 90) {
	x -= 55;
      } else {
	x = 38;
      }

      m = 60000 - 32768 + x;

      m = (m * 128) + power + 2 + 64;
    } else if (callsign[slash_pos + 3] == ' ' || callsign[slash_pos + 3] == 0) {
      // Two-digit numerical suffix
      char base_call[7];
      memset(base_call, 0, 7);
      strncpy(base_call, callsign, slash_pos);
      for (int i = 0; i < 6; i++) {
	base_call[i] = toupper(base_call[i]);
	if (!(isdigit(base_call[i]) || isupper(base_call[i]))) base_call[i] = ' ';
      }

      padCallsign(base_call);

      n = wsprCode(base_call[0]);
      n = n * 36 + wsprCode(base_call[1]);
      n = n * 10 + wsprCode(base_call[2]);
      n = n * 27 + (wsprCode(base_call[3]) - 10);
      n = n * 27 + (wsprCode(base_call[4]) - 10);
      n = n * 27 + (wsprCode(base_call[5]) - 10);

      // TODO: needs validation of digit
      m = 10 * (callsign[slash_pos + 1] - 48) + callsign[slash_pos + 2] - 48;
      m = 60000 + 26 + m;
      m = (m * 128) + power + 2 + 64;
    } else {
      // Prefix
      char prefix[4];
      char base_call[7];
      memset(prefix, 0, 4);
      memset(base_call, 0, 7);
      strncpy(prefix, callsign, slash_pos);
      strncpy(base_call, callsign + slash_pos + 1, 7);

      if (prefix[2] == ' ' || prefix[2] == 0) {
	// Right align prefix
	prefix[3] = 0;
	prefix[2] = prefix[1];
	prefix[1] = prefix[0];
	prefix[0] = ' ';
      }

      for (int i = 0; i < 6; i++) {
	base_call[i] = toupper(base_call[i]);
	if (!(isdigit(base_call[i]) || isupper(base_call[i]))) base_call[i] = ' ';
      }

      padCallsign(base_call);

      n = wsprCode(base_call[0]);
      n = n * 36 + wsprCode(base_call[1]);
      n = n * 10 + wsprCode(base_call[2]);
      n = n * 27 + (wsprCode(base_call[3]) - 10);
      n = n * 27 + (wsprCode(base_call[4]) - 10);
      n = n * 27 + (wsprCode(base_call[5]) - 10);

      m = 0;
      for (int i = 0; i < 3; ++i) m = 37 * m + wsprCode(prefix[i]);

      if (m >= 32768) {
	m -= 32768;
	m = (m * 128) + power + 2 + 64;
      } else {
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


void WSPREncoder::interleave(uint8_t * s) {
  uint8_t d[MSGSIZE];
  uint8_t rev, index_temp;
  int sIndex = 0;

  for (int j = 0; j < 255; j++) {
    // Bit reverse the index
    index_temp = j;
    rev = 0;

    for (int k = 0; k < 8; k++) {
      if (index_temp & 0x01) rev = rev | (1 << (7 - k));
      index_temp = index_temp >> 1;
    }

    if (rev < MSGSIZE) d[rev] = s[sIndex++];
    if (sIndex >= MSGSIZE) break;
  }

  memcpy(s, d, MSGSIZE);
}


void WSPREncoder::mergeSyncVector(uint8_t * g) {
  static const uint8_t sync_vector[MSGSIZE] = {
    1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0,
    1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0,
    0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 1,
    0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0,
    1, 1, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1,
    0, 0, 1, 0, 0, 1, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1,
    1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0,
  };

  for (int i = 0; i < MSGSIZE; i++) txBuf[i] = sync_vector[i] + (2 * g[i]);
}

/*
 * JTEncode.h - JT65/JT9/WSPR/FSQ encoder library for ESP32
 *
 * Copyright (C) 2015-2021 Jason Milldrum <milldrum@gmail.com>
 *
 * Based on the algorithms presented in the WSJT software suite.
 * Thanks to Andy Talbot G4JNT for the whitepaper on the WSPR encoding
 * process that helped me to understand all of this.
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

#ifndef JTENCODE_H
#define JTENCODE_H

#include "int.h"
#include "rs_common.h"
#include "nhash.h"

#include <stdint.h>


template<unsigned SPACING, auto DELAY, unsigned long DEFAULT_FREQ, unsigned MSGSIZE>
class JTEncoder {
public:
  static inline const uint16_t spacing = SPACING;
  static inline const typeof(DELAY) delay = DELAY;

  JTEncoder(uint32_t txFreq = DEFAULT_FREQ)
    : freq(txFreq)
  {}

  virtual void encode() = 0;
  uint32_t freq;

protected:
  virtual void mergeSyncVector(const uint8_t *g) = 0;
  virtual void bitPacking(uint8_t *bitsP);
  virtual void interleave(uint8_t *s);
  uint8_t txBuf[MSGSIZE];
};


class WSPREncoder: JTEncoder<146, 683, 14097200UL, 162> {
public:
  virtual void encode(const char *callP, const char *locP, const uint8_t dbm);

protected:
  void messagePrep(const char *callP, const char *locP, const uint8_t dbm);
};

/*
class JTEncoder<174, 576, 14078700UL, 85, 206>: JT9Encoder;
class JTEncoder<269, 371, 14078300UL, 126>: JT65Encoder;
class JTEncoder<437, 229, 14078500UL, 207, 206>: JT4Encoder;
class JTEncoder<625, 159, 14075000UL, 79, 174>: FT8Encoder;
class JTEncoder<879, std::tuple(500, 333, 222, 167), 14078300UL, 0>: FSQEncoder;

  void jt65_encode(const char * msg, uint8_t * symbols);
  void jt9_encode(const char * message, uint8_t * symbols);
  void jt4_encode(const char * message, uint8_t * symbols);
  void wspr_encode(const char * call, const char * loc, const int8_t dbm, uint8_t * symbols);
  void fsq_encode(const char * from_call, const char * message, uint8_t * symbols);
  void fsq_dir_encode(const char * from_call, const char * to_call,
		      const char cmd, const char * message, uint8_t * symbols);
  void ft8_encode(const char * message, uint8_t * symbols);
  void latlon_to_grid(float lat, float lon, char* ret_grid);


private:
  uint8_t jt_code(char);
  uint8_t ft_code(char);
  uint8_t wspr_code(char);
  uint8_t gray_code(uint8_t);
  int8_t hex2int(char);
  void jt_message_prep(char *);
  void ft_message_prep(char *);
  void wspr_message_prep(char *, char *, int8_t);
  void jt65_bit_packing(char *, uint8_t *);
  void jt9_bit_packing(char *, uint8_t *);
  void wspr_bit_packing(uint8_t *);
  void ft8_bit_packing(char*, uint8_t*);
  void jt65_interleave(uint8_t *);
  void jt9_interleave(uint8_t *);
  void wspr_interleave(uint8_t *);
  void jt9_packbits(uint8_t *, uint8_t *);
  void jt_gray_code(uint8_t *, uint8_t);
  void ft8_encode(uint8_t*, uint8_t*);
  void jt65_merge_sync_vector(uint8_t *, uint8_t *);
  void jt9_merge_sync_vector(uint8_t *, uint8_t *);
  void jt4_merge_sync_vector(uint8_t *, uint8_t *);
  void wspr_merge_sync_vector(uint8_t *, uint8_t *);
  void ft8_merge_sync_vector(uint8_t*, uint8_t*);
  void convolve(uint8_t *, uint8_t *, uint8_t, uint8_t);
  void rs_encode(uint8_t *, uint8_t *);
  void encode_rs_int(void *,data_t *, data_t *);
  void free_rs_int(void *);
  void * init_rs_int(int, int, int, int, int, int);
  uint8_t crc8(const char *);
  void pad_callsign(char *);
  void * rs_inst;
  char callsign[12];
  char locator[7];
  int8_t power;
};
*/
#endif

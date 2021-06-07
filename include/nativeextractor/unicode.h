// Copyright (C) 2021 SpongeData s.r.o.
//
// NativeExtractor is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// NativeExtractor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with NativeExtractor. If not, see <http://www.gnu.org/licenses/>.

#ifndef UNICODE_H
#define UNICODE_H

#include <nativeextractor/common.h>

static inline void print_binary(unsigned char b) {
  uint8_t mask = 0b10000000;
  for (uint8_t i = 0; i < 8; ++i) {
    putchar((b & mask) ? '1' : '0');
    mask = mask >> 1;
  }
}

/**
 * Infers utf-8 symbol length in bytes e.g. 1-4
 *
 * @param c   an utf-8 string
 *
 * @returns   1-4
 */
static inline size_t unicode_getbytesize(char* c) {
  switch ((uint8_t)*c & (uint8_t)0b11110000) {
    case 0b11110000: return 4;
    case 0b11100000: return 3;
    case 0b11000000: return 2;
    case 0b11010000: return 2;
  }
  return 1;
}

/** Converts a unicode charater into a single integer value. */
static inline uint32_t unicode_to_int(char *c) {
  uint32_t encoded = 0;
  for (int i = unicode_getbytesize(c) - 1; i >= 0; --i) {
    encoded = (encoded << 8) | (uint8_t)*(c++);
  }
  return encoded;
}

/**
 * Prints first character from given unicode string.
 *
 * @param c The unicode string.
 */
static inline void print_unicode(char* c) {
  for (int i = unicode_getbytesize(c) - 1; i >= 0; --i) {
    putchar(*(c++));
  }
}

static inline void inspect_unicode(char* u) {
  printf("Unicode:\t\"");
  print_unicode(u);
  printf("\"\n");

  uint32_t bytesize = unicode_getbytesize(u);
  printf("Bytesize:\t%u\n", bytesize);

  // Bytes
  printf("Bytes:\t\t");
  for (int i = 0; i < bytesize; ++i) {
    print_binary(u[i]);
    putchar(' ');
  }
  putchar('\n');

  // Encoded
  uint32_t encoded = unicode_to_int(u);
  printf("Encoded:\t%u\n", encoded);
  printf("Encoded bytes:\t");
  print_binary((encoded & (uint32_t)0xFF0000) >> 24);
  putchar(' ');
  print_binary((encoded & (uint32_t)0x00FF0000) >> 16);
  putchar(' ');
  print_binary((encoded & (uint32_t)0x0000FF00) >> 8);
  putchar(' ');
  print_binary(encoded & (uint32_t)0x000000FF);
  putchar(' ');

  putchar('\n');
  putchar('\n');
}

/**
 * Compares two utf-8 symbols
 *
 * @param u1    an utf-8 string
 * @param u2    an utf-8 string
 *
 * @returns     true if symbols are equal, false otherwise
 */
static inline bool cmp_unicode(char* u1, char* u2) {
  // return (unicode_to_int(u1) == unicode_to_int(u2));

  size_t bs1 = unicode_getbytesize(u1);
  size_t bs2 = unicode_getbytesize(u2);

  if (bs1 != bs2) {
    return false;
  }

  for (size_t i = 0; i < bs1; ++i) {
    if (u1[i] != u2[i]) return false;
  }

  return true;
}

extern bool unicode_isalnum(char* c);

extern bool unicode_not_isalnum(char* c);

extern bool unicode_isalpha(char* c);

extern bool unicode_not_isalpha(char* c);

extern bool unicode_iscntrl(char* c);

extern bool unicode_not_iscntrl(char* c);

extern bool unicode_isdigit(char* c);

extern bool unicode_not_isdigit(char* c);

extern bool unicode_isgraph(char* c);

extern bool unicode_not_isgraph(char* c);

extern bool unicode_islower(char* c);

extern bool unicode_not_islower(char* c);

extern bool unicode_isprint(char* c);

extern bool unicode_not_isprint(char* c);

extern bool unicode_ispunct(char* c);

extern bool unicode_not_ispunct(char* c);

extern bool unicode_isspace(char* c);

extern bool unicode_not_isspace(char* c);

extern bool unicode_isupper(char* c);

extern bool unicode_not_isupper(char* c);

extern bool unicode_isxdigit(char* c);

extern bool unicode_not_isxdigit(char* c);

extern bool unicode_islinebreak(char* c);

extern bool unicode_not_islinebreak(char* c);

extern bool unicode_isw(char* c);

extern bool unicode_not_isw(char* c);

#endif // UNICODE_H

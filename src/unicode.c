/**
 * Copyright (C) 2021 SpongeData s.r.o.
 *
 * NativeExtractor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * NativeExtractor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with NativeExtractor. If not, see <http://www.gnu.org/licenses/>.
 */

#include <nativeextractor/unicode.h>
#include <string.h>
#include <glib-2.0/glib.h>

#define GEN_UNICODE_PREDICATE(fn) \
  bool unicode_##fn(char* c) { \
    return g_unichar_##fn(g_utf8_get_char(c)); \
  }\
  bool unicode_not_##fn(char* c) { \
    return !g_unichar_##fn(g_utf8_get_char(c)); \
  }

GEN_UNICODE_PREDICATE(isalnum)

GEN_UNICODE_PREDICATE(isalpha)

GEN_UNICODE_PREDICATE(iscntrl)

GEN_UNICODE_PREDICATE(isdigit)

GEN_UNICODE_PREDICATE(isgraph)

GEN_UNICODE_PREDICATE(islower)

GEN_UNICODE_PREDICATE(isprint)

GEN_UNICODE_PREDICATE(ispunct)

GEN_UNICODE_PREDICATE(isspace)

GEN_UNICODE_PREDICATE(isupper)

GEN_UNICODE_PREDICATE(isxdigit)

bool unicode_islinebreak(char *c) {
  return (*c == '\n');
}

bool unicode_not_islinebreak(char *c) {
  return !unicode_islinebreak(c);
}

bool unicode_isw(char *c) {
  return (unicode_isalnum(c) || *c == '_');
}

bool unicode_not_isw(char *c) {
  return !unicode_isw(c);
}

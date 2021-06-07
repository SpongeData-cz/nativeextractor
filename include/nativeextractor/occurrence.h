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

#ifndef OCCURRENCE_H
#define OCCURRENCE_H

#include <nativeextractor/common.h>

typedef struct occurrence_t {
  char* str;
  uint64_t pos;
  uint64_t upos;
  uint32_t len;
  uint32_t ulen;
  const char * label;
  float prob;
} occurrence_t;

void print_pos(occurrence_t * p);

#endif // OCCURRENCE_H

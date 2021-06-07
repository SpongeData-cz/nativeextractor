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

#ifndef PAIR_H
#define PAIR_H

#include <nativeextractor/common.h>

typedef struct pair_t {
  void* car;
  void* cdr;
  void (*destroy)(struct pair_t* self);
} pair_t;

pair_t* pair_create(void* car, void* cdr);

void pair_init(pair_t* self, void* car, void* cdr);

#endif // PAIR_H

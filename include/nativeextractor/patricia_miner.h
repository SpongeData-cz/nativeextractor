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

#ifndef PATRICIA_MINER_C
#define PATRICIA_MINER_C

#include <nativeextractor/miner.h>
#include <nativeextractor/patricia.h>

typedef struct patricia_miner_c {
  miner_c base;
  patricia_c *patricia;
  void (*destroy_super)(miner_c *self);
} patricia_miner_c;

patricia_miner_c *patricia_miner_c_create(const char* name, void* params, matcher_t matcher);

void patricia_miner_c_destroy(patricia_miner_c *self);

#endif // PATRICIA_MINER_C

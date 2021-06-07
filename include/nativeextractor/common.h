// Copyright (C) 2021 mafiosso
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

#ifndef COMMON_H
#define COMMON_H

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stddef.h>

#define CONTAINER_OF(addr, type, member) \
  ((type*)((void*)addr - offsetof(type, member)))

#define ALLOC(o) ((o*) malloc(sizeof(o)))

#define DESTROY(o) do { (o)->destroy(o); free(o); } while (0)

#ifndef MIN
  #define MIN(a, b) ((a) <= (b) ? (a) : (b))
#endif

#ifndef MAX
  #define MAX(a, b) ((a) >= (b) ? (a) : (b))
#endif

#define CMP(a, b) (((a) < (b)) ? -1 : (((a) == (b)) ? 0 : +1))

#define SIGN(x) CMP(x, 0)

#ifdef DEBUG
#define PRINT_DEBUG(...) printf(__VA_ARGS__)
#else
#define PRINT_DEBUG(...)
#endif

#endif // COMMON_H

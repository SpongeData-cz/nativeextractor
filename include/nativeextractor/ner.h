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

#ifndef NER_H
#define NER_H

#include <nativeextractor/miner.h>
#include <nativeextractor/patricia.h>

typedef struct ner_c {
  miner_c_BODY
  patricia_c* index;
} ner_c;

ner_c* ner_c_create(const char* name, patricia_c* index);

void ner_c_init(ner_c* self, const char* name, patricia_c* index);

#endif // NER_H

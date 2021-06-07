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

#include <nativeextractor/occurrence.h>
#include <string.h>

void print_pos(occurrence_t * p) {
  char str[p->len + 1];
  str[p->len] = '\0';
  memcpy(str, p->str, sizeof(char) * p->len);
  printf("(occurrence_t){.label: \"%s\", .pos: %ld, .upos: %ld, .len: %d, .prob %.3f, .str: \"%s\"}\n",
    p->label, p->pos, p->upos, p->len, p->prob, str);
}

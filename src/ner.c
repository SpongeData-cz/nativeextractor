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

#include <nativeextractor/ner.h>

occurrence_t* match_named_entity(miner_c* e) {
  ner_c* ner = (ner_c*)e;
  patricia_c* index = ner->index;

  if (MATCH_DELIMITER(e, Left, Right)
    && e->mark_start(e)
    && e->match_fn_plus(e, unicode_not_isspace, Right)
    && MATCH_DELIMITER(e, Right, Stay)
    && e->mark_end(e)) {
    uint32_t length = (uint32_t)(e->end - e->start);
    uint32_t match = index->search(index, e->start, length);
    if ((float)match / (float)length >= 0.75) {
      PRINT_DEBUG("Matches %u / %u\n", match, length);
      return e->make_occurrence(e, 1.0);
    }
  }
  return NULL;
}

void ner_c_destroy(ner_c* self) {
  miner_c_destroy((miner_c*)self);
}

ner_c* ner_c_create(const char* name, patricia_c* index) {
  ner_c* ner = ALLOC(ner_c);
  ner_c_init(ner, name, index);
  return ner;
}

void ner_c_init(ner_c* self, const char* name, patricia_c* index) {
  miner_c_init((miner_c*)self, name, NULL, match_named_entity);
  self->index = index;
  self->destroy = (void(*)(miner_c*))ner_c_destroy;
}

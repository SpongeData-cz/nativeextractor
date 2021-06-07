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

#include <nativeextractor/patricia_miner.h>

patricia_miner_c *patricia_miner_c_create(const char* name, void* params, matcher_t matcher) {
  patricia_miner_c *miner = ALLOC(patricia_miner_c);
  miner_c_init(&(miner->base), name, NULL, matcher);
  miner->destroy_super = miner->base.destroy;
  miner->base.destroy = (void (*)(miner_c *self))patricia_miner_c_destroy;
  miner->patricia = (patricia_c *)params;
  return miner;
}

void patricia_miner_c_destroy(patricia_miner_c *self) {
  DESTROY(self->patricia);
  self->destroy_super((miner_c *)self);
}

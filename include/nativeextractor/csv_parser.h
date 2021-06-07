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

#ifndef CSV_PARSER_H
#define CSV_PARSER_H

#include <nativeextractor/common.h>
#include <nativeextractor/stream.h>
#include <nativeextractor/miner.h>
#include <gmodule.h>

#define CSV_IS_NEWLINE(value) \
  (value == NULL)

typedef struct csv_batch_c {
  GByteArray* bytes;
  guint8* iter;
  void(*destroy)(struct csv_batch_c* self);
} csv_batch_c;

csv_batch_c* csv_batch_create();

void csv_batch_init(csv_batch_c* self);

void csv_batch_destroy(csv_batch_c* self);

void csv_batch_add_occurrence(csv_batch_c* self, occurrence_t* o);

void csv_batch_add_add_newline(csv_batch_c* self);

bool csv_batch_has_next(csv_batch_c* self);

char* csv_batch_get_next(csv_batch_c* self);

typedef struct csv_parser_c {
  stream_c* stream;
  miner_c miner;
  /** A symbol used to separate values. */
  char* delimiter;
  /** A symbol used to wrap values containing a delimiter. */
  char* quote;
  /** If true then the last created occurrence ended with a delimiter. */
  bool was_delimiter;
  /** If true then the last created occurrence ended with a newline. */
  bool was_newline;
  /** If true then the last created occurrence ended with an EOF. */
  bool was_eof;
  void (*destroy)(struct csv_parser_c* self);
} csv_parser_c;

csv_parser_c* csv_parser_create(stream_c* stream);

void csv_parser_init(csv_parser_c* self, stream_c* stream);

void csv_parser_destroy(csv_parser_c* self);

/**
 * @param batch_size Maximum number of lines to read. Use 0 for no limit.
 */
csv_batch_c* csv_parser_parse(csv_parser_c* self, size_t batch_size);

#endif // CSV_PARSER_H

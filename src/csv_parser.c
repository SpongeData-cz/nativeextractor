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

#include <string.h>
#include <nativeextractor/csv_parser.h>
#include <nativeextractor/unicode.h>

////////////////////////////////////////////////////////////////////////////////
// Batch

csv_batch_c* csv_batch_create() {
  csv_batch_c* batch = ALLOC(csv_batch_c);
  csv_batch_init(batch);
  return batch;
}

void csv_batch_init(csv_batch_c* self) {
  self->bytes = g_byte_array_new();
  self->iter = NULL;
  self->destroy = csv_batch_destroy;
}

void csv_batch_destroy(csv_batch_c* self) {
  g_byte_array_free(self->bytes, TRUE);
}

void csv_batch_add_occurrence(csv_batch_c* self, occurrence_t* o) {
  GByteArray* bytes = self->bytes;

  size_t len = o->len + 1;
  g_byte_array_append(bytes, (guint8*)&len, sizeof(size_t));

  char s[len];
  s[len - 1] = '\0';
  strncpy(s, o->str, len - 1);
  g_byte_array_append(bytes, (guint8*)s, len);
}

void csv_batch_add_newline(csv_batch_c* self) {
  size_t len = 0;
  g_byte_array_append(self->bytes, (guint8*)&len, sizeof(size_t));
}

bool csv_batch_has_next(csv_batch_c* self) {
  return (self->iter < self->bytes->data + self->bytes->len);
}

char* csv_batch_get_next(csv_batch_c* self) {
  GByteArray* bytes = self->bytes;

  if (!self->iter) {
    self->iter = bytes->data;
  }

  if (!(csv_batch_has_next(self))) {
    return NULL;
  }

  size_t len = (size_t)*self->iter;
  self->iter += sizeof(size_t);

  if (len > 0) {
    char* str = (char*)self->iter;
    self->iter += sizeof(char) * len;
    return str;
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Parser

csv_parser_c* csv_parser_create(stream_c* stream) {
  csv_parser_c* parser = ALLOC(csv_parser_c);
  csv_parser_init(parser, stream);
  return parser;
}

// See: https://tools.ietf.org/html/rfc4180
occurrence_t* match_csv(miner_c* m) {
  csv_parser_c* parser = CONTAINER_OF(m, csv_parser_c, miner);
  char* quote = parser->quote;
  char* delimiter = parser->delimiter;
  char double_quote[] = { *quote, *quote, '\0' };

  if (m->match(m, quote, Right)) {
    // Match quoted string
    if (!m->mark_start(m)) return NULL;

    while (m->can_move(m, Right)) {
      if (m->match_string(m, double_quote, Right)) {
        continue;
      }
      if (cmp_unicode(m->get_next(m), quote)) {
        break;
      }
      m->move(m, Right);
    }

    if (!m->mark_end(m)) return NULL;
    if (!m->match(m, quote, Right)) return NULL;
  }
  else  {
    // Match normal string
    if (!m->mark_start(m)) return NULL;

    while (m->can_move(m, Right)) {
      char* next = m->get_next(m);
      if (cmp_unicode(next, quote)
        || cmp_unicode(next, delimiter)
        || cmp_unicode(next, "\n")
        || cmp_unicode(next, "\r")) {
        break;
      }
      m->move(m, Right);
    }

    if (!m->mark_end(m)) return NULL;
  }

  if (m->match(m, delimiter, Right)) {
    parser->was_newline = false;
    parser->was_delimiter = true;
    parser->was_eof = false;
    return m->make_occurrence(m, 1.0f);
  }

  if (m->match_string(m, "\r\n", Right)
    || m->match(m, "\n", Right)
    || m->match(m, "\r", Right)) {
    parser->was_newline = true;
    parser->was_delimiter = false;
    parser->was_eof = false;
    return m->make_occurrence(m, 1.0f);
  }

  if (!m->can_move(m, Right)) {
    parser->was_newline = false;
    parser->was_delimiter = false;
    parser->was_eof = true;
    return m->make_occurrence(m, 1.0f);
  }

  return NULL;
}

void csv_parser_init(csv_parser_c* self, stream_c* stream) {
  self->stream = stream;

  miner_c_init(&self->miner, "CSV", NULL, match_csv);
  self->miner.allow_empty = true;
  self->miner.set_stream(&self->miner, stream);

  self->delimiter = ",";
  self->quote = "\"";
  self->destroy = csv_parser_destroy;
}

void csv_parser_destroy(csv_parser_c* self) {
  miner_c_destroy(&self->miner);
}

csv_batch_c* csv_parser_parse(csv_parser_c* self, size_t batch_size) {
  miner_c* miner = &self->miner;
  self->was_delimiter = false;
  self->was_newline = false;
  self->was_eof = false;

  csv_batch_c* batch = csv_batch_create();
  size_t row_count = 0;

  while (!(miner->stream->state_flags & STREAM_EOF)) {
    occurrence_t* o = miner->run(miner);
    if (o) {
      csv_batch_add_occurrence(batch, o);
      free(o);
      if (self->was_newline || self->was_eof) {
        csv_batch_add_newline(batch);
        if (++row_count >= batch_size && batch_size > 0) {
          break;
        }
      }
    } else {
      if (!(miner->stream->state_flags & STREAM_EOF)) {
        printf("The CSV file was not entirely parsed!\n");
      }
      break;
    }
  }

  if (self->was_delimiter) {
    miner->mark_start(miner);
    miner->mark_end(miner);
    occurrence_t* o = miner->make_occurrence(miner, 1.0f);
    csv_batch_add_occurrence(batch, o);
    free(o);
  }

  if (row_count == 0) {
    DESTROY(batch);
    return NULL;
  }

  return batch;
}

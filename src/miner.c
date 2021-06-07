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
#include <pthread.h>
#include <dlfcn.h>
#include <nativeextractor/miner.h>
#include <nativeextractor/terminal.h>
#include <nativeextractor/unicode.h>

char* strdup(const char*);

occurrence_t* miner_c_run(miner_c* self) {
  occurrence_t* o = self->matcher(self);
  self->pos_last = (self->end > self->stream->pos) ? self->end : self->stream->pos;
  self->start = NULL;
  self->start_unicode = 0;
  self->end = NULL;
  self->end_unicode = 0;
  return o;
}

bool miner_c_mark_start(miner_c* self) {
  if (self->stream->pos < self->end_last) {
    return false;
  }
  self->start = self->stream->pos;
  self->start_unicode = self->stream->unicode_offset;
  return true;
}

bool miner_c_mark_end(miner_c* self) {
  if (self->stream->pos < self->end_last) {
    return false;
  }
  self->end = self->stream->pos;
  self->end_unicode = self->stream->unicode_offset;
  return true;
}

bool miner_c_mark_pos(miner_c* self, mark_t* mark) {
  mark->pos = self->stream->pos;
  mark->unicode_offset = self->stream->unicode_offset;
  mark->state_flags = self->stream->state_flags;
  return true;
}

bool miner_c_reset_pos(miner_c* self, mark_t* mark) {
  self->stream->pos = mark->pos;
  self->stream->unicode_offset = mark->unicode_offset;
  stream_c_normalize_position(self->stream);
  // self->stream->state_flags = mark->state_flags;
  return true;
}

bool miner_c_can_move(miner_c* self, dir_e move) {
  switch (move) {
    case Left:
      return !(self->stream->state_flags & STREAM_BOF);

    case Right:
      return !(self->stream->state_flags & STREAM_EOF);

    default:
      return true;
  }
}

bool miner_c_move(miner_c* self, dir_e move) {
  switch (move) {
    case Left:
      self->stream->prev_char(self->stream);
      break;

    case Right:
      self->stream->next_char(self->stream);
      break;

    default:
      break;
  }
  return true;
}

char* miner_c_get_next(miner_c* self) {
  // Prints progress
#if 0
  static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&mutex);
  printf("%s: \t", self->name);
  uint64_t i = 0;
  uint64_t current_offset = self->stream->unicode_offset;
  char* pos = self->stream->start;
  while (pos != self->stream->end) {
    char* color = NULL;
    if (self->start != NULL) {
      if (i >= self->start_unicode
        && i <= ((self->end != NULL) ? self->end_unicode : current_offset)) {
        color = TC1(TC_B_GREEN);
      }
    }
    if (i == current_offset) { color = TC1(TC_B_CYAN); }
    if (color != NULL) { printf("%s", color); }
    if (unicode_isspace(pos)) {
      putchar(' ');
    } else {
      print_unicode(pos);
    }
    if (color != NULL) { printf("%s", TC1(TC_RESET)); }
    pos += unicode_getbytesize(pos);
    ++i;
  }
  pthread_mutex_unlock(&mutex);
  putchar('\n');
#endif

  return self->stream->pos;
}

bool miner_c_match_fn(miner_c* self, match_fn_t fn, dir_e move) {
  char* match_last = self->stream->pos;

  bool retval = (self->can_move(self, move)
    && fn(self->get_next(self))
    && self->move(self, move));

  if (retval) {
    self->match_last = match_last;
  }

  return retval;
}

static inline bool _match_fn_mod(miner_c* self, match_fn_t fn, dir_e move, bool has_match) {
  char* match_last = NULL;

  while (true) {
    char* _match = self->stream->pos;
    if (!(self->can_move(self, move)
      && fn(self->get_next(self))
      && self->move(self, move))) {
      break;
    }
    match_last = _match;
    has_match = true;
  }

  if (match_last != NULL) {
    self->match_last = match_last;
  }

  return has_match;
}

bool miner_c_match_fn_plus(miner_c* self, match_fn_t fn, dir_e move) {
  return _match_fn_mod(self, fn, move, false);
}

bool miner_c_match_fn_star(miner_c* self, match_fn_t fn, dir_e move) {
  return _match_fn_mod(self, fn, move, true);
}

bool miner_c_match_fn_times(miner_c* self, match_fn_t fn, dir_e move, int times) {
  mark_t mark;
  self->mark_pos(self, &mark);

  char* match_last;

  for (int i = 0; i < times; ++i) {
    match_last = self->stream->pos;
    if (!(self->can_move(self, move)
      && fn(self->get_next(self))
      && self->move(self, move))) {
      self->reset_pos(self, &mark);
      return false;
    }
  }

  self->match_last = match_last;
  return true;
}

bool miner_c_match(miner_c* self, char* chr, dir_e move) {
  bool retval = (self->can_move(self, move)
    && cmp_unicode(self->get_next(self), chr)
    && self->move(self, move));

  if (retval) {
    self->match_last = chr;
  }

  return retval;
}

bool is_delimiter(char* c) {
  return (unicode_isspace(c)
    || unicode_ispunct(c)
    || unicode_iscntrl(c));
}

bool miner_c_match_delimiter(miner_c* self, dir_e move) {
  return self->match_fn(self, is_delimiter, move);
}

bool miner_c_match_string(miner_c* self, const char* str, dir_e move) {
  assert(move != Stay);
  assert(move != Left); // TODO: Implement reverse string matching!
  mark_t start;
  self->mark_pos(self, &start);
  size_t i = 0;
  while (str[i] != '\0') {
    if (!self->match(self, (char*) &str[i], move)) {
      self->reset_pos(self, &start);
      return false;
    }
    i += unicode_getbytesize((char*) &str[i]);
  }
  return true;
}

bool miner_c_match_one(miner_c* self, const char* str, dir_e move) {
  if (!self->can_move(self, move)) return false;
  size_t i = 0;
  while (str[i] != '\0') {
    if (cmp_unicode(self->stream->pos, (char*) &str[i])) {
      char* match_last = self->stream->pos;
      if (self->move(self, move)) {
        self->match_last = match_last;
        return true;
      }
      break;
    }
    i += unicode_getbytesize((char*) &str[i]);
  }
  return false;
}

occurrence_t* miner_c_make_occurrence(miner_c* self, float prob) {
  if (self->start == NULL) {
    PRINT_DEBUG("Trying to create '%s' occurrence without start marked!\n", self->name);
    return NULL;
  }

  if (self->end == NULL) {
    PRINT_DEBUG("Trying to create '%s' occurrence without end marked!\n", self->name);
    return NULL;
  }

  if (!self->allow_empty && self->start == self->end) {
    PRINT_DEBUG("Trying to create '%s' occurrence of 0 length!\n", self->name);
    return NULL;
  }

  if (self->start > self->end) {
    PRINT_DEBUG("Invalid '%s' occurrence - start must be positioned before end!\n", self->name);
    return NULL;
  }

  self->end_last = self->end;

  occurrence_t o = (occurrence_t) {
    .str = self->start,
    .pos = self->start - self->stream->start,
    .upos = self->start_unicode,
    .len = self->end - self->start,
    .ulen = self->end_unicode - self->start_unicode,
    .prob = prob,
    .label = self->name,
  };

  occurrence_t* retval = ALLOC(occurrence_t);
  memcpy(retval, &o, sizeof(occurrence_t));
  return retval;
}

miner_c* miner_c_create(const char* name, void* params, matcher_t matcher) {
  miner_c* m = ALLOC(miner_c);
  miner_c_init(m, name, params, matcher);
  return m;
}

void miner_c_set_stream(miner_c* self, stream_c* stream) {
  memcpy(self->stream, stream, sizeof(stream_c));
  self->match_last = NULL;
  self->start = NULL;
  self->end = NULL;
  self->end_last = NULL;
  self->pos_last = NULL;
}

void miner_c_destroy(miner_c* self) {
  free(self->stream);
}

void miner_c_init(miner_c* self, const char* name, void* params, matcher_t matcher) {
  self->name = name;
  self->params = params;
  self->stream = calloc(1, sizeof(stream_c));
  self->match_last = NULL;
  self->start = NULL;
  self->end = NULL;
  self->end_last = NULL;
  self->pos_last = NULL;
  self->allow_empty = false;
  self->matcher = matcher;

  self->destroy = miner_c_destroy;
  self->set_stream = miner_c_set_stream;
  self->run = miner_c_run;
  self->mark_start = miner_c_mark_start;
  self->mark_end = miner_c_mark_end;
  self->mark_pos = miner_c_mark_pos;
  self->reset_pos = miner_c_reset_pos;
  self->can_move = miner_c_can_move;
  self->move = miner_c_move;
  self->get_next = miner_c_get_next;
  self->match_fn = miner_c_match_fn;
  self->match_fn_plus = miner_c_match_fn_plus;
  self->match_fn_star = miner_c_match_fn_star;
  self->match_fn_times = miner_c_match_fn_times;
  self->match = miner_c_match;
  self->match_delimiter = miner_c_match_delimiter;
  self->match_string = miner_c_match_string;
  self->match_one = miner_c_match_one;
  self->make_occurrence = miner_c_make_occurrence;
}

char** extract_meta(const char* path) {
  void *so = dlopen(path, RTLD_LAZY);

  if (so == NULL) {
    return NULL;
  }

  const char** meta = dlsym(so, "meta");
  size_t metalen;
  for (metalen = 0; meta[metalen] != NULL; ++metalen) {};

  char** meta2 = malloc((metalen + 1) * sizeof(char*));
  for (size_t i = 0; i < metalen; ++i) {
    meta2[i] = strdup(meta[i]);
  }
  meta2[metalen] = NULL;

  dlclose(so);
  return meta2;
}

void free_meta(char** meta) {
  for (size_t i = 0; meta[i] != NULL; ++i) {
    free(meta[i]);
  }
  free(meta);
}
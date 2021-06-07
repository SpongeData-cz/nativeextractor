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

#include <nativeextractor/stream.h>
#include <nativeextractor/unicode.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int stream_open(stream_file_c * self, const char * fullpath){
  self->fd = open(fullpath, O_RDONLY);

  if (self->fd < 0) {
    self->stream.state_flags |= STREAM_FAILED;
    return self->stream.state_flags;
  }

  self->stream.fsize = lseek(self->fd, 0, SEEK_END);

  lseek(self->fd, 0, SEEK_SET);

  int flags = MAP_PRIVATE;
  #ifdef MAP_NORESERVE
  flags |= MAP_NORESERVE;
  #endif

  // TODO: URL parsing does not work when read is used instead of mmap
  /*if( self->fsize > 0 && self->fsize < STREAM_FREAD_LIMIT){
    self->start = calloc(self->fsize + 1, sizeof(char));
    self->state_flags |= STREAM_MALLOC;
    if (read(self->fd, self->start, self->fsize) == -1) {
      self->state_flags |= STREAM_FAILED;
      return self->state_flags;
    }
    lseek(self->fd, 0, SEEK_SET);
  }
  else */if( self->stream.fsize > 0 ){
    self->stream.start = (char*)mmap(NULL, self->stream.fsize, PROT_READ, flags, self->fd, 0);

    self->stream.state_flags |= STREAM_MMAP;

    if (self->stream.start == MAP_FAILED) {
      close(self->fd);
      self->stream.state_flags |= STREAM_FAILED;
      return self->stream.state_flags;
    }
  }
  else{
    self->stream.start = NULL;
    self->stream.state_flags |= STREAM_EOF;
  }

  self->stream.pos = self->stream.start;
  self->stream.end = self->stream.start + self->stream.fsize;
  self->stream.unicode_offset = 0;
  self->stream.state_flags |= (STREAM_INITED | STREAM_BOF);

  if( self->stream.pos == self->stream.end ){
    self->stream.state_flags |= STREAM_EOF;
  }

  return self->stream.state_flags;
}

void stream_c_sync(stream_c* self, stream_c* stream) {
  self->pos = stream->pos;
  self->unicode_offset = stream->unicode_offset;
  self->state_flags = stream->state_flags;
}

static inline int64_t _move(stream_c* self, int64_t m) {
  char* current_pos = self->pos;
  uint64_t unicode_offset_prev = self->unicode_offset;

  switch (SIGN(m)) {
    case -1:
      // Move left
      for (int64_t i = m; i < 0; ++i) {
        if (self->state_flags & STREAM_BOF) break;

        while (true) {
          --current_pos;
          if (*current_pos < (char)0b10000000 || *current_pos >= (char)0b11000000) break;
        }
        self->pos = current_pos;

        stream_c_normalize_position(self);

        if (self->pos > current_pos) {
          current_pos = self->pos;
          break;
        }
        --self->unicode_offset;
      }
      break;

    case +1:
      // Move right
      for (int64_t i = 0; i < m; ++i) {
        if (self->state_flags & STREAM_EOF) break;

        current_pos += unicode_getbytesize(self->pos);
        self->pos = current_pos;

        stream_c_normalize_position(self);

        if (self->pos < current_pos) {
          current_pos = self->pos;
          break;
        }
        ++self->unicode_offset;
      }
      break;

    default:
      break;
  }

  return (int64_t)self->unicode_offset - (int64_t)unicode_offset_prev;
}

int64_t stream_c_move(stream_c* self, int64_t m) {
  return _move(self, m);
}

char * stream_c_next_char(stream_c * self){
  char * out = self->pos;
  _move(self, +1);
  return out;
}

char * stream_c_prev_char(stream_c * self){
  char * out = self->pos;
  _move(self, -1);
  return out;
}

void stream_c_destroy(stream_c* self){
  self->state_flags = STREAM_DONE;
}

stream_c * stream_c_new(){
  stream_c * out = calloc(1, sizeof(stream_c));

  out->sync = stream_c_sync;
  out->move = stream_c_move;
  out->next_char = stream_c_next_char;
  out->prev_char = stream_c_prev_char;
  out->destroy = stream_c_destroy;

  return out;
}

void stream_file_c_destroy(stream_file_c * self){
  self->stream.destroy(&(self->stream));

  if (self->fd != -1) {
    close(self->fd);
  }

  if (self->stream.state_flags & STREAM_MMAP) {
    munmap(self->stream.start, self->stream.fsize);
  }
}

stream_file_c * stream_file_c_new(const char * path) {
  stream_file_c * out = calloc(1, sizeof(stream_file_c));
  stream_c * stream = stream_c_new();
  out->stream = *stream;
  free(stream);

  out->open_file = stream_open;
  out->fd = -1;
  out->destroy = stream_file_c_destroy;

  out->open_file(out, path);
  return out;
}

int stream_buffer_c_open_buffer(stream_buffer_c * self, const uint8_t * buffer, size_t buff_sz){
  self->stream.fsize = buff_sz;

  if( buffer && self->stream.fsize > 0 ){
    self->stream.state_flags |= STREAM_MALLOC;
    self->stream.start = (char*)buffer;
  }
  else{
    self->stream.start = NULL;
    self->stream.state_flags |= STREAM_EOF;
  }

  self->stream.pos = self->stream.start;
  self->stream.end = self->stream.start + self->stream.fsize;
  self->stream.unicode_offset = 0;
  self->stream.state_flags |= (STREAM_INITED | STREAM_BOF);

  if( self->stream.pos == self->stream.end ){
    self->stream.state_flags |= STREAM_EOF;
  }

  return self->stream.state_flags;
}

stream_buffer_c * stream_buffer_c_new(const uint8_t * buffer, size_t buff_sz) {
  stream_buffer_c * out = calloc(1, sizeof(stream_buffer_c));
  stream_c * stream = stream_c_new();
  out->stream = *stream;
  out->open_buffer = stream_buffer_c_open_buffer;
  out->open_buffer(out, buffer, buff_sz);
  out->destroy = stream_c_destroy;
  free(stream);

  return out;
}


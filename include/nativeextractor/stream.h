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

#ifndef STREAM_H
#define STREAM_H

#include <nativeextractor/common.h>

#define STREAM_FREAD_LIMIT 32000000
#define STREAM_UNDEFINED   0
#define STREAM_BOF         1
#define STREAM_EOF         (1 << 1)
#define STREAM_INITED      (1 << 2)
#define STREAM_FAILED      (1 << 3)
#define STREAM_PROCESSING  (1 << 4)
#define STREAM_DONE        (1 << 5)
#define STREAM_MMAP        (1 << 6)
#define STREAM_MALLOC      (1 << 7)

/**
 * Base class for streams. Movements respects memory limits.

*/
typedef struct stream_c {
  /**
   * State flags:
   *
   * @param STREAM_BOF Current position is on the stream begining.
   * @param STREAM_EOF  Current positions is on the stream ending.
   * @param STREAM_INITED Stream already inited.
   * @param STREAM_FAILED Stream failed to initialize, movements have undefined behaviour.
   * @param STREAM_PROCESSING Undocumented state.
   * @param STREAM_DONE Undocumented state.
   * @param STREAM_MMAP Undocumented state.
   * @param STREAM_MALLOC Undocumented state.
   */
  unsigned state_flags; /* flags */
  off_t fsize; /* size of stream in bytes */
  uint64_t unicode_offset; /* logical unicode offset */

  char * start; /* base pointer */
  char * pos; /* last_valid pointer (>= start) */
  char * end;

  /**
   * Moves stream to a next valid utf-8 char. Limits are detected (under/over flows). Check state_flags for limits.
   *
   * @param self Self pointer of type stream_c.
   *
   * @returns New position in stream, starting on next valid unicode char or behind stream.
  */
  char * (*next_char)(struct stream_c * self);
  /**
   * Moves stream to a previous valid utf-8 char. Limits are detected (under/over flows). Check state_flags for limits.
   *
   * @param self Self pointer of type stream_c.
   *
   * @returns New position in stream, starting on next valid unicode char or behind stream.
  */
  char * (*prev_char)(struct stream_c * self);
  /**
   * Moves stream to a valid utf-8 char in some distance from current position. Limits are detected (under/over flows). Check state_flags for limits.
   *
   * @param self Self pointer of type stream_c.
   * @param m Offset of movement, negative numbers possible with natural meaning.
   *
   * @returns New position in stream, starting on next valid unicode char or behind stream.
  */
  int64_t (*move)(struct stream_c * self, int64_t m);
  /**
   * Update movement parameters according to the second stream.
   *
   * @param self Self pointer of type stream_c.
   * @param stream Another object of type stream_c. Movement parameters are copied from this.
  */
  void (*sync)(struct stream_c* self, struct stream_c* stream);

  /**
   * Standard "IDestroyable" implementation.
   *
   * @param self Self pointer of type stream_c.
  */
  void (*destroy)(struct stream_c* self);
} stream_c;

/**
 * Class representing stream from file via mmap. Inherits from stream_c.
*/
typedef struct stream_file_c {
  /** Class parent. */
  stream_c stream;
  /** Unix file descriptor. */
  int fd;

  /** Opens a file and initializes itself as a valid stream.
   *
   * @param self Object of type stream_file_c.
   * @param fullpath A path to the file.
   *
   * @returns State flags from itself - should be checked on errors. Never contains STREAM_FAILED if passed.
  */
  int (*open_file)(struct stream_file_c * self, const char * fullpath);
  /**
   * Standard "IDestroyable" implementation.
   *
   * @param self Self pointer of type stream_c.
  */
  void (*destroy)(struct stream_file_c* self);
} stream_file_c;


/* Class representing buffer over heap memory. Subclass of stream_c. */
typedef struct stream_buffer_c {
  stream_c stream;
  /**
   * Intializes stream from s buffer of length buff_sz. Uses user-memory, user is responsible for freeing it.
   *
   *  @param self
   *  @param buffer A memory place.
   *  @param buff_sz This size will be taken as buffer size. You can obviously use smaller size than real size of the buffer.
   *
   * @returns State flags.
  */
  int (*open_buffer)(struct stream_buffer_c * self, const uint8_t * buffer, size_t buff_sz);
  /**
   * Standard "IDestroyable" implementation.
   *
   * @param self Self pointer of type stream_c.
  */
  void (*destroy)(struct stream_c* self);
} stream_buffer_c;

/** Stream_c constructor.
 *
 * @returns A valid stream_c object stream. Use DESTROY(stream) to free.
*/
stream_c * stream_c_new();

/** Stream_file_c constructor.
 *
 * @param path Path to a mmap-able file.
 *
 * @returns A valid stream_c object stream. Use DESTROY(stream) to free.
*/
stream_file_c * stream_file_c_new(const char * path);

/** Stream_file_c constructor.
 *
 * @param buffer A heap allocated memory pointer.
 * @param buff_sz Size of that memory (can be smaller obviously).
 *
 * @returns A valid stream_c object stream. Use DESTROY(stream) to free.
*/
stream_buffer_c * stream_buffer_c_new(const uint8_t * buffer, size_t buff_sz);

static inline void stream_c_normalize_position(stream_c * self){
  self->state_flags &= (~STREAM_EOF & ~STREAM_BOF);
  if (self->pos >= self->end) {
    self->pos = self->end;
    self->state_flags |= STREAM_EOF;
  } else if (self->pos <= self->start) {
    self->pos = self->start;
    self->state_flags |= STREAM_BOF;
  }
}

#endif // STREAM_H

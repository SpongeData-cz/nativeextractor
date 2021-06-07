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

#ifndef MINER_H
#define MINER_H

#include <nativeextractor/common.h>
#include <nativeextractor/occurrence.h>
#include <nativeextractor/stream.h>

#define MATCH_DELIMITER(e, side, move) \
  ((e)->match_delimiter(e, move) || !(e)->can_move(e, side))

struct miner_c;

typedef struct mark_t {
  char* pos;
  uint64_t unicode_offset;
  unsigned state_flags;
} mark_t;

typedef enum dir_e {
  Left = -1,
  Stay = 0,
  Right = 1
} dir_e;

typedef occurrence_t* (*matcher_t)(struct miner_c* extractor);

typedef bool (*match_fn_t)(char* c);

#define miner_c_BODY                                                           \
   /** The name of the extractor. */                                           \
  const char* name;                                                            \
                                                                               \
   /** The parameters. */                                                      \
  void* params;                                                          \
                                                                               \
  /** The stream to process. */                                                \
  stream_c* stream;                                                            \
                                                                               \
  /** Last matched char. */                                                    \
  char* match_last;                                                            \
                                                                               \
  /** Starting position of an occurrence within the stream. */                 \
  char* start;                                                                 \
                                                                               \
  /** Starting position of an occurrence within the stream as a unicode offset
   * in bytes. */                                                              \
  uint64_t start_unicode;                                                      \
                                                                               \
  /** Ending position of an occurrence within the stream. */                   \
  char* end;                                                                   \
                                                                               \
  /** Ending position of an occurrence within the stream as a unicode offset
   * in bytes. */                                                              \
  uint64_t end_unicode;                                                        \
                                                                               \
  /** Ending position of the last occurrence within the stream. */             \
  char* end_last;                                                              \
                                                                               \
  /** The farthest where the miner moved in the stream in previous run. */     \
  char* pos_last;                                                              \
                                                                               \
  /** If true then make_occurrence can create empty occurrences. Defaults to
   * false. */                                                                 \
  bool allow_empty;                                                            \
                                                                               \
  /** A function for finding occurrences. */                                   \
  matcher_t matcher;                                                           \
                                                                               \
  /**
   * Frees resources used by a miner from memory.
   *
   * @param self An instance of a miner.
   */                                                                          \
  void (*destroy)(struct miner_c* self);                                       \
                                                                               \
  /**
   * Defines stream in which a miner will find occurrences.
   *
   * @param self An instance of a miner.
   * @param stream An instance of a stream.
   */                                                                          \
  void (*set_stream)(struct miner_c* self, stream_c* stream);                  \
                                                                               \
  /**
   * Executes the miner's matcher function and returns found occurrence.
   *
   * @param self An instance of a miner.
   *
   * @return The found occurrence or NULL if no occurrence was found.
   */                                                                          \
  occurrence_t* (*run)(struct miner_c* self);                                  \
                                                                               \
  /**
   * Marks current position in the stream as the starting position of an
   * occurrence. Fails if the current position is before the end position of
   * the last created occurrence.
   *
   * @param self An instance of a miner.
   *
   * @return True on success.
   */                                                                          \
  bool (*mark_start)(struct miner_c* self);                                    \
                                                                               \
  /**
   * Marks current position in the stream as the ending position of an
   * occurrence.
   *
   * @param self An instance of a miner.
   *
   * @return True on success.
   */                                                                          \
  bool (*mark_end)(struct miner_c* self);                                      \
                                                                               \
  /**
   * Stores current position in the stream into a mark structure.
   *
   * @param self An instance of a miner.
   * @param mark A mark structure which will hold the position.
   *
   * @return True on success.
   */                                                                          \
  bool (*mark_pos)(struct miner_c* self, mark_t* mark);                        \
                                                                               \
  /**
   * Sets position in the stream to the position stored in a mark structure.
   *
   * @param self An instance of a mark.
   * @param mark A structure holding a position in the stream.
   *
   * @return True on success.
   */                                                                          \
  bool (*reset_pos)(struct miner_c* self, mark_t* mark);                       \
                                                                               \
  /**
   * @param self An instance of a miner.
   * @param move A direction to move in.
   *
   * @return True if the miner can move in given direction.
   */                                                                          \
  bool (*can_move)(struct miner_c* self, dir_e move);                          \
                                                                               \
  /**
   * Moves the current position in the stream in given direction.
   *
   * @param self An instance of a miner.
   * @param move A direction to move in.
   *
   * @return True on success.
   *
   * @note This does not check whether it is actually possible to move in given
   * direction! Use method `can_move` to avoid segmentation errors if not sure!
   */                                                                          \
  bool (*move)(struct miner_c* self, dir_e move);                              \
                                                                               \
  /**
   * @param self An instance of a miner.
   *
   * @return Next character from the stream.
   */                                                                          \
  char* (*get_next)(struct miner_c* self);                                     \
                                                                               \
  /**
   * Checks whether predicate function `fn` returns true for current character
   * in the stream. If it does, then moves in given direction, otherwise stays
   * at the same position.
   *
   * @param self An instance of a miner.
   * @param fn A predicate function.
   * @param move A direction to move in if a character is matched.
   *
   * @return True if a character was matched.
   */                                                                          \
  bool (*match_fn)(struct miner_c* self, match_fn_t fn, dir_e move);           \
                                                                               \
  /**
   * Similar to `match_fn`, but matches at least one or more characters, just as
   * + does in regular expressions.
   *
   * @param self An instance of a miner.
   * @param fn A predicate function.
   * @param move A direction to move in after each matched character.
   *
   * @return True if at least one character was matched.
   */                                                                          \
  bool (*match_fn_plus)(struct miner_c* self, match_fn_t fn, dir_e move);      \
                                                                               \
  /**
   * Similar to `match_fn`, but matches zero or more characters, just as * does
   * in regular expressions.
   *
   * @param self An instance of a miner.
   * @param fn A predicate function.
   * @param move A direction to move in after each matched character.
   *
   * @return Returns true on success.
   */                                                                          \
  bool (*match_fn_star)(struct miner_c* self, match_fn_t fn, dir_e move);      \
                                                                               \
  /**
   * Similar to `match_fn`, but matches given amount of characters, just as
   * {times} would do in regular expressions.
   *
   * @param self An instance of a miner.
   * @param fn A predicate function.
   * @param move A direction to move in after each matched character.
   * @param times Number of characters to match.
   *
   * @returns True if given number of characters was matched.
   */                                                                          \
  bool (*match_fn_times)(struct miner_c* self, match_fn_t fn, dir_e move, int times); \
                                                                               \
  /**
   * Checks whether current character in the stream matches the passed character.
   * If it does, then moves in specified direction, otherwise stays at the same
   * position.
   *
   * @param self An instance of a miner.
   * @param chr A character to match.
   * @param move A direction to move in if the characters match.
   *
   * @return True if the characters match.
   */                                                                          \
  bool (*match)(struct miner_c* self, char* chr, dir_e move);                  \
                                                                               \
  /**
   * Checks whether the current character in the stream is a control character,
   * a punctuation or a whitespace. If it is, then moves in specified direction,
   * otherwise stays at the same position.
   *
   * @param self An instance of a miner.
   * @param move A direction to move in if the character is a delimiter.
   *
   * @return True if the character is a delimiter.
   */                                                                          \
  bool (*match_delimiter)(struct miner_c* self, dir_e move);                   \
                                                                               \
  /**
   * Consecutively matches each character from a string with characters in the
   * stream in specified direction. Right means the string will be matched from
   * left to right, moving right in the stream after each matched character.
   * Left means the string will be matched from right to left, moving left in
   * the stream after each matched character.
   *
   * @param self An instance of a miner.
   * @param str A string to match.
   * @param move A direction to match the string in.
   *
   * @return True if the whole string was matched.
   *
   * @note Only right matching is currently supported!
   */                                                                          \
  bool (*match_string)(struct miner_c* self, const char* str, dir_e move);     \
                                                                               \
  /**
   * Checks whether the current character in the stream matches any one
   * character from a string. If it does, then moves in specified direction,
   * otherwise stays at the same position.
   *
   * @param self An instance of a miner.
   * @param str String of character options.
   * @param move A direction to move in if any one character is matched.
   *
   * @return True if any one of character from the string was matched.
   */                                                                          \
  bool (*match_one)(struct miner_c* self, const char* str, dir_e move);        \
                                                                               \
  /**
   * Creates a new occurrence. Methods `mark_start` and `mark_end` must be
   * called first to define the start and the end of the occurrence. If the
   * occurrence would intersect with the last created occurrence, then it is not
   * created.
   *
   * @param self An instance of a miner.
   * @param prob FIXME: Add param description.
   *
   * @return The created occurrence or NULL.
   */                                                                          \
  occurrence_t* (*make_occurrence)(struct miner_c* self, float prob);

typedef struct miner_c {
  miner_c_BODY
} miner_c;

/**
 * Creates a new miner instance.
 *
 * @param name The name of the miner.
 * @param params Parameters for the miner.
 * @param matcher A function which matches and returns found occurrences.
 *
 * @return The created miner.
 */
miner_c* miner_c_create(const char* name, void* params, matcher_t matcher);

/**
 * Initializes an instance of a miner. Normally this is done automatically in a
 * constructor, so call this only in initializer functions of another classes
 * which inherit from the miner class.
 *
 * @param self An instance of a miner.
 * @param name The name of the miner.
 * @param params Parameters for the miner.
 * @param matcher A function which matches and returns found occurrences.
 *
 * @return The created miner.
 */
void miner_c_init(miner_c* self, const char* name, void* params, matcher_t matcher);

void miner_c_destroy(miner_c* self);

bool is_delimiter(char* c);

char** extract_meta(const char* path);

void free_meta(char** meta);

#endif // MINER_H

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

#include <nativeextractor/miner.h>
#include <nativeextractor/unicode.h>

#include <gmodule.h>

/**
 * Checks if expr is false, then skips miner until EOF or next delimiter
 * and returns NULL.
 */
#define retnul(expr, miner) do {\
  if ((expr)) {\
    while ((miner)->can_move((miner), Right)\
        && !(miner)->match_delimiter((miner), Right)) {\
      (miner)->move((miner), Right);\
    }\
    return NULL;\
  }\
} while (false)


static bool match_character(miner_c* m, char* character) {
  gunichar c = unicode_to_int(character);
  gunichar lc = g_unichar_tolower(c);
  gunichar uc = g_unichar_toupper(c);

  return m->match(m, (char*)&lc, Right) || m->match(m, (char*)&uc, Right);
}

static bool match_any_character(miner_c* m) {
  bool ret = !(m->match_delimiter(m, Stay));
  if (ret) {
    m->move(m, Right);
  }
  return ret;
}

static bool match_any_string(miner_c* m, const char* glob) {
  // TODO
  return false;
}

static bool match_range(miner_c* m, char* from, char* to) {
  int end = unicode_to_int(to);
  for (int codepoint = unicode_to_int(from); codepoint <= end; ++codepoint) {
    if (match_character(m, (char*)&codepoint)) {
      return true;
    }
  }
  return false;
}

static bool starts_with_delimiter(const char* glob) {
  char* c = (char*)glob;

  switch (*c) {
    case '[': {
      bool escape = false;
      while (true) {
        if (*c == '\0') {
          return false;
        }

        if (*c == ']' && !escape) {
          return false;
        }

        if (*c == '\\' && !escape) {
          escape = true;
          ++c;
          continue;
        }
        escape = false;

        if (is_delimiter(c)) {
          return true;
        }

        ++c;
      }
      assert(false);
    }

    case '*':
    case '?':
      return false;

    case '\\':
      ++c;
      break;
  }

  return is_delimiter(c);
}

/** Matches glob patterns. */
static occurrence_t* match_glob_impl(miner_c* m) {
  char* glob = m->params;
  const char* end = glob + strlen(glob);

  bool start = false;

  if (!starts_with_delimiter(glob)) {
    // skip to next token
    while (m->can_move(m, Right) && m->match_delimiter(m, Right)) {}
  }

  retnul(!m->can_move(m, Right), m);

  mark_t startpos;

  while (glob < end) {
    if (!start) {
      start = true;
      m->mark_start(m);
      m->mark_pos(m, &startpos);
    }

    int ch = unicode_to_int(glob);
    size_t chlen = unicode_getbytesize(glob);

    bool no_special = false;
    if (chlen == 1) {
      switch ((char) ch) {
        case '*':
          // if this is the end of the glob, just match everything
          if (glob[1] == '\0') {
            while (m->can_move(m, Right) && !m->match_delimiter(m, Stay)) {
              m->move(m, Right);
            }
            break;
          }

          mark_t pos;
          const char* save_params = m->params;
          const char* new_params = glob + 1;
          const char* save_end_last = m->end_last;

          while (true) {
            m->mark_pos(m, &pos);
            m->params = (void*)new_params;
            occurrence_t* rec = match_glob_impl(m);
            m->params = (void*)save_params;
            m->end_last = (char*)save_end_last;

            if (rec) {
              free(rec);
              mark_t t;
              m->mark_pos(m, &t);
              m->reset_pos(m, &startpos);
              m->mark_start(m);
              m->reset_pos(m, &t);
              occurrence_t* ret = m->make_occurrence(m, 1.0);
              return ret;
            }

            m->reset_pos(m, &pos);
            if (!m->can_move(m, Right)) {
              return NULL;
            }
            m->move(m, Right);

            retnul(m->match_delimiter(m, Stay), m);
          }
          break;

        case '[':
          ++glob; // throw away left bracket
          bool found = false;
          char* last = NULL;
          do {
            chlen = unicode_getbytesize(glob);

            if (!found) {
              if ((last != NULL)
                  && (*last == '\\')) {
                if (match_character(m, glob)) {
                  found = true;
                }
              } else if ((chlen == 1)
                  && (*glob == '-')) {
                char* first = last;
                char* last = glob + 1;
                if (match_range(m, first, last)) {
                  found = true;
                }
                ++glob; // throw away dash
                chlen = unicode_getbytesize(glob);
              } else {
                if (match_character(m, glob)) {
                  found = true;
                }
              }
            }

            last = glob;
            glob += chlen;
          } while (*glob != ']' || *last == '\\');

          chlen = 1; // size of right bracket

          retnul(!found, m);

          break;

        case '\\':
          retnul(!match_character(m, glob + chlen), m);
          glob += unicode_getbytesize(glob + chlen);
          break;

        case '?':
          retnul(!match_any_character(m), m);
          break;

        default:
          no_special = true;
      }
    } else {
      no_special = true;
    }

    retnul(no_special && !match_character(m, glob), m);

    glob += chlen;
  }

  m->mark_end(m);

  // if the token doesn't end here
  retnul(!m->match_delimiter(m, Right) && m->can_move(m, Right), m);

  return m->make_occurrence(m, 1.0);
}

/** Returns true if string is a syntactically correct glob. */
static bool is_glob(const char* glob) {
  int brackets = 0;
  bool escape = false;
  char* prelast = NULL;
  char* last = NULL;

  for (char* p = (char*)glob; *p != '\0'; ++p) {
    if (escape) {
      escape = false;
      continue;
    }

    switch (*p) {
      case '-':
        if ((brackets > 0)
            && (last && (*last == '-')
                || prelast && (*prelast == '-'))) {
          return false;
        }
        break;
      case '\\':
        escape = true;
        break;
      case '[':
        ++brackets;
        break;
      case ']':
        --brackets;
        if (brackets < 0) {
          return false; // fail early for speed
        }
        break;
      default:
        // do nothing
        break;
    }
    prelast = last;
    last = p;
  }

  if (brackets != 0) {
    return false;
  }

  return true;
}

/** Returns a miner which matches glob patterns. */
miner_c* match_glob(const char* glob) {
  if (!is_glob(glob)) {
    fprintf(stderr, "'%s' is not a syntactically correct glob!\n", glob);
    return NULL;
  }
  return miner_c_create("Glob", (void*)glob, match_glob_impl);
}

const char* meta[] = {
  "match_glob", "Glob",
  NULL
};

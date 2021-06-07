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

// Do not include runtime headers if compiling as .so module
#ifndef SO_MODULE
#include <nativeextractor/common.h>
#include <nativeextractor/extractor.h>
#endif

/* character filter function accepting alnum chars and -, ., _ */
static bool match_email_valid_chars(char * c) {
  if(unicode_isalnum(c)) return true;

  switch(*c) {
    case '-':
    case '.':
    case '_':
      return true;

    default:
      return false;
  }
}

/* matches the same as match_email_valid_chars except for '.' */
static bool match_domain_chars(char * c) {
  return match_email_valid_chars(c) && *c != '.';
}

/* validates left context (before @ from the left) */
static bool validate_left_context(miner_c * m) {
  // moves head to the left (one char before @)
  m->move(m, Left); // <- @ ...

  // applies filter function match_email_valid_chars form Right to the Left
  // passes when at least one valid char is present
  if(m->match_fn_plus(m, match_email_valid_chars, Left)) {
    // marks start as the current possition (the leftmost one)
    m->mark_start(m);
    // passess
    return true;
  }

  // fails
  return false;
}

static bool validate_domain(miner_c * m) {
  // move to the right behind the @
  m->move(m, Right); // ... @ -> .TLD<separator>
  // subdomain counters
  unsigned subdomains = 0;
  // last subdomain begining
  mark_t last_subdomain_start;
  // save current position
  m->mark_pos(m, &last_subdomain_start);

  while(true) {
    // while matching domain chars move to the right
    while(m->match_fn(m, match_domain_chars, Right)) {}

    // domain char not present currently
    // if current char is . then start new subdomain and continue
    if(m->match_one(m, ".", Right)) {
      // increase subdomain counter
      subdomains++;
      // save position of the subdomain begining
      m->mark_pos(m, &last_subdomain_start);
    }
    else {
      // otherwise end subdomains
      subdomains++;
      // mark potential end
      m->mark_end(m);
      // stop
      break;
    }
  }

  // at least 2 subdomains should be present (for example email.com)
  if(subdomains < 2) {
    // fail
    return false;
  }

  // save end position
  mark_t end;
  m->mark_pos(m, &end);

  // if the last subdomain have less than 2 chars, fail
  if((end.pos - last_subdomain_start.pos) < 2) {
    return false;
  }

  // pass
  return true;
}

/** Matches glob patterns. */
static occurrence_t* match_email_naive_impl(miner_c* m) {
  // Most specific character when matching emails is @
  // When @ seen analyze left context (email user) and right context (domain)
  // Head stays on the current position of the at-sign
  if (m->match(m, "@", Stay)) {
    // create at_sign pos for future use
    mark_t at_sign_pos;
    // save current head's position as at_sign_pos
    m->mark_pos(m, &at_sign_pos);

    // validate left context (email user)
    if(!validate_left_context(m)) {
      // if it fails return NULL - no occurrence
      return NULL;
    }

    // reset position back to at-sign
    m->reset_pos(m, &at_sign_pos);

    // analyze right context (domain)
    if(!validate_domain(m)) {
      return NULL;
    }

    // left and right context passed, return occurrence with confidence 1.0
    return m->make_occurrence(m, 1.0);
  }

  // no at-sign seen
  return NULL;
}

/** Returns a miner which matches glob patterns. */
miner_c* match_email_naive(const char* args) {
  // instantiate new miner_c easily and return
  return miner_c_create("Email", NULL, match_email_naive_impl);
}

/* if not defined SO_MODULE define main entrypoint */
#ifndef SO_MODULE
int main(unsigned argc, char** argv) {
  if (argc <= 1) {
    printf("Fullpath not specified!\n");
    return EXIT_FAILURE;
  }

  // create stream from file
  stream_file_c * sfc = stream_file_c_new(argv[1]);

  // create miners - a NULL terminated array of miner_c *
  miner_c ** miners = calloc(2, sizeof(miner_c*));
  // add naive email miner
  miners[0] = match_email_naive(NULL);

  // create extractor_c with 1 thread and one miner
  extractor_c * e = extractor_c_new(1, miners);
  // set the file stream to the extractor
  e->set_stream(e, (stream_c*)sfc);

  // scan stream until not ended
  while (!((e->stream->state_flags) & STREAM_EOF)) {
    // take batch from stream of size 1000000B at max
    occurrence_t ** res = e->next(e, 1000000);
    // pointer to a NULL terminated array of occurrences
    occurrence_t ** pres = res;
    // iterate of occurrences
    while (*pres) {
      // print occurrence
      print_pos(*pres);
      // free occurrence - important
      free(*pres);
      ++pres;
    }
    // free occurrence list - important
    free(res);
  }

  // destroy the extractor gently
  DESTROY(e);
  // destroy file stream correctly
  DESTROY(sfc);

  return EXIT_SUCCESS;
}
#else
/* Otherwise define module meta info - build as .so module */
const char* meta[] = {
  "match_email_naive", "Email",
  NULL
};
#endif
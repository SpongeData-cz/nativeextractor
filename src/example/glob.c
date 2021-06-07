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

// Simple example using implemented globs
#include <nativeextractor/common.h>
#include <nativeextractor/extractor.h>

const char * help_msg = "Glob miner example use\n" \
  "./build/debug/glob glob file\n" \
  "\tglob - glob in format (SYMBOL|*|?|[SYMBOL1SYMBOL2]|[SYMBOL1-SYMBOL2])+\n" \
  "\twhere:" \
  "\n\t\tSYMBOL - an unicode symbol\n" \
  "\t\t* - Kleene closure\n" \
  "\t\t? - Any single char\n" \
  "\t\t[SYMBOL1..SYMBOLN] - Some set containing SYMBOL1 to SYMBOLN\n" \
  "\t\t[SYMBOL1-SYMBOLN] - Set containing symbols from range SYMBOL1 to SYMBOLN - using natural linear character ordering.\n"
  "\n\tfile - path to a unicode textual file"
  ;

int main(int argc, char ** argv) {
  if (argc <= 2) {
    printf("Fullpath and glob not specified!\n");
    printf("%s\n", help_msg);
    return EXIT_FAILURE;
  }

  // Alloc empty miners array
  miner_c ** miners = calloc(1, sizeof(miner_c*));
  const char * glob = argv[1];
  const char * filename = argv[2];

  // Instantiate extractor
  extractor_c * e = extractor_c_new(1, miners);
  // Add glob miner
  if( !e->add_miner_so(e, "./build/debug/lib/glob_entities.so", "match_glob", (void*)glob)) {
    printf("Error adding miner %s\n", e->get_last_error(e));
    return EXIT_FAILURE;
  }

  // Open file stream
  stream_file_c * sfc = stream_file_c_new(filename);
  // Set the stream to the extractor
  e->set_stream(e, (stream_c*)sfc);

  // Iterate through the stream
  while (!((e->stream->state_flags) & STREAM_EOF)) {
    occurrence_t ** res = e->next(e, 1000000);
    occurrence_t ** pres = res;
    // Print found occurrences
    while (*pres) {
      print_pos(*pres);
      free(*pres);
      ++pres;
    }
    free(res);
  }

  // Destroy extractor and the file stream gently
  DESTROY(e);
  DESTROY(sfc);
  return EXIT_SUCCESS;
}

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

#define _GNU_SOURCE

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include <stdlib.h>

#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#include <nativeextractor/common.h>
#include <nativeextractor/extractor.h>
#include <nativeextractor/miner.h>
#include <nativeextractor/ner.h>
#include <nativeextractor/occurrence.h>
#include <nativeextractor/patricia.h>
#include <nativeextractor/stream.h>
#include <nativeextractor/unicode.h>

#ifdef DEBUG
  #define LIBSTR "./build/debug/lib/"
#else
  #define LIBSTR "./build/release/lib/"
#endif

#define MAX_EXTRACTORS (256)
#define GLOB_TEST_DIR "glob_test_dir"

/**
 * The created extractors for future cleanup.
 */
extractor_c *extractors[MAX_EXTRACTORS];
/**
 * Current number of extractors.
 */
size_t extractors_count = 0;
/**
 * Maximum number of extractors.
 */
size_t extractors_size = sizeof extractors / sizeof *extractors;

/**
 * Makes an extractor with a single miner and notes it in the extractors
 * array for cleanup.
 *
 * @param so_dir the *.so file
 * @param symb the symbol
 * @param params the parameters
 *
 * @return the extractor
 */
extractor_c *make_extractor(
  const char *so_dir,
  const char *symb,
  const char *params
) {
  if (extractors_count >= extractors_size) {
    assert(false);
  }
  extractor_c *extractor = extractor_c_new(1, NULL);
  extractors[extractors_count++] = extractor;
  extractor->add_miner_so(extractor, so_dir, symb, (void*)params);
  return extractor;
}

/**
 * Creates an extractor with a single glob miner.
 *
 * @param glob the glob pattern
 *
 * @return the extractor
 */
extractor_c *make_glob(const char *glob) {
  return make_extractor(LIBSTR "glob_entities.so", "match_glob", glob);
}

/**
 * Creates a text file with the specified contents.
 *
 * @param contents the intended contents
 *
 * @return the fullpath
 */
const char *make_file(const char *contents) {
  const char *fullpath = GLOB_TEST_DIR "/" "test.txt";
  FILE *f = fopen(fullpath, "w");
  fputs(contents, f);
  fclose(f);
  return fullpath;
}

/**
 * Returns the number of occurrences.
 *
 * @param occurrences an array of occurrences
 *
 * @return the number of occurrences
 */
size_t occurrence_len(occurrence_t **occurrences) {
  size_t ret = 0;
  while (occurrences[ret] != NULL) {
    ++ret;
  }
  return ret;
}

/**
 * Deallocates an array of occurrences.
 *
 * @param occurrences the array of occurrences
 */
void free_occurrences(occurrence_t **occurrences) {
  occurrence_t ** ptr = occurrences;

  while (*ptr) {
    #ifdef DEBUG
    print_pos(*ptr);
    #endif // DEBUG
    free(*ptr);
    ++ptr;
  }

  free(occurrences);
}

/**
 * Tests if a glob pattern is found a specific number of times in a file.
 * Fails the cmocka test if not.
 *
 * @param glob the glob pattern
 * @param fullpath the file
 * @param matches intended number of matches
 */
void test_glob(const char *glob, const char *fullpath, size_t matches) {
  extractor_c *ex = make_glob(glob);

  stream_file_c * s = stream_file_c_new(fullpath);
  ex->set_stream(ex, (stream_c*)s);
  occurrence_t **res = ex->next(ex, 1000);
  assert_int_equal(occurrence_len(res), matches);
  free_occurrences(res);

  DESTROY(s);
}

/**
 * Creates a directory for temporary files.
 */
void init(void) {
  if (mkdir(GLOB_TEST_DIR, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
    if (errno != EEXIST) {
      fprintf(stderr, "Error: %s\n", strerror(errno));
      assert(false);
    }
  }
}

/**
 * Tests exact matching.
 *
 * @param arg whatever cmocka passes here
 */
void exact_match(void **arg) {
  const char *fullpath = make_file("abc abcdef abc");
  test_glob("abc", fullpath, 2);
  test_glob("abcdef", fullpath, 1);
  test_glob("def", fullpath, 0);
  test_glob("abc abcdef abc", fullpath, 1);
}

/**
 * Tests character range matching.
 *
 * @param arg whatever cmocka passes here
 */
void character_range(void **arg) {
  const char *fullpath = make_file("bat cat mat lat");
  test_glob("[bclm]at", fullpath, 4);
}

/**
 * Tests single character wildcard matching.
 *
 * @param arg whatever cmocka passes here
 */
void any_character_wildcard(void **arg) {
  const char *fullpath = make_file("bat mat mad pat lat lot lit");
  test_glob("l?t", fullpath, 3);
  test_glob("?a?", fullpath, 5);
}

/**
 * Tests variable length wildcard matching.
 *
 * @param arg whatever cmocka passes here
 */
void any_string_wildcard(void **arg) {
  const char *fullpath = make_file(
    "Twinkle twinkle little star "
    "I want to hit you with a car "
    "Throw you off a cliff so high "
    "Hope you break your neck and die"
  );
  test_glob("*", fullpath, 26);
  test_glob("*kle", fullpath, 2);
  test_glob("T*kle", fullpath, 2);
  test_glob("*i*k*", fullpath, 2);
}

/**
 * Tests escape matching.
 *
 * @param arg whatever cmocka passes here
 */
void escape(void **arg) {
  const char *fullpath = make_file("[abc]de ade bde cde");
  test_glob("\\a\\d\\e", fullpath, 1);
  test_glob("\\[abc\\]de", fullpath, 1);
  test_glob("[\\[\\]\\*\\?]", make_file("["), 1);
}

/**
 * Tests mixed cases.
 *
 * @param arg whatever cmocka passes here
 */
void mixed(void **arg) {
  const char *fullpath = make_file("russel");
  test_glob("[pqrstabc]?*l", fullpath, 1);

  fullpath = make_file(
    "awliefduzs78bxc "
    "dfueilq234zdhiu "
    "2w45ry7uu7748ju8778"
  );
  test_glob("*[abc]*[a-f]*[1-9]*", fullpath, 1);
}

/**
 * Destroys created extractors and deletes created files.
 */
void cleanup(void) {
  printf("Destroying %ld extractors.\n", extractors_count);
  extractor_c **ptr = extractors;
  while (*ptr != NULL) {
    //DESTROY(*ptr);
    (*ptr)->destroy(*ptr);
    free(*ptr);
    ++ptr;
  }

  #define Popen system
  const char *cmd = "rm -rf " GLOB_TEST_DIR;
  puts(cmd);
  Popen(cmd);
}

/**
 * Test entrypoint.
 *
 * @param argc the number of arguments
 * @param argv the arguments
 *
 * @return whatever cmocka returns
 */
int main(int argc, char *argv[]) {
  init();

  const struct CMUnitTest tests[] = {
    cmocka_unit_test(exact_match),
    cmocka_unit_test(character_range),
    cmocka_unit_test(any_character_wildcard),
    cmocka_unit_test(any_string_wildcard),
    cmocka_unit_test(escape),
    cmocka_unit_test(mixed),
  };

  atexit(cleanup);

  return cmocka_run_group_tests(tests, NULL, NULL);
}

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
#include <nativeextractor/regex_generator.h>
#include <nativeextractor/stream.h>
#include <nativeextractor/unicode.h>

#ifdef DEBUG
  #define LIBSTR "./build/debug/lib/"
#else
  #define LIBSTR "./build/release/lib/"
#endif

#define MAX_EXTRACTORS (256)
#define TEST_DIR "test_dir/"

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
  const char **globs
) {
  if (extractors_count >= extractors_size) {
    assert(false);
  }
  extractor_c *extractor = extractor_c_new(1, NULL);
  extractors[extractors_count++] = extractor;
  while (*globs != NULL) {
    extractor->add_miner_so(
      extractor,
      LIBSTR "glob_entities.so",
      "match_glob",
      (void*)(*globs)
    );
    ++globs;
  }
  return extractor;
}

/**
 * Creates a text file with the specified contents.
 *
 * @param contents the intended contents
 *
 * @return the fullpath
 */
const char *make_file(const char *contents) {
  const char *fullpath = TEST_DIR "/" "test.txt";
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
 * Prints an array of occurrences.
 *
 * @param occurrences the array of occurrences
 */
void print_occurrences(occurrence_t **occurrences) {
  occurrence_t ** ptr = occurrences;

  while (*ptr) {
    print_pos(*ptr);
    ++ptr;
  }
}

/**
 * Creates a directory for temporary files.
 */
void init(void) {
  if (mkdir(TEST_DIR, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
    if (errno != EEXIST) {
      fprintf(stderr, "Error: %s\n", strerror(errno));
      assert(false);
    }
  }
}

void test_match(
  extractor_c *extractor,
  size_t batch,
  const char *fullpath,
  size_t matches
) {
  stream_file_c * s = stream_file_c_new(fullpath);
  extractor->set_stream(extractor, (stream_c*)s);

  size_t found = 0;
  while (!((extractor->stream->state_flags) & STREAM_EOF)) {
    occurrence_t **res = extractor->next(extractor, batch);
    occurrence_t **pres = res;

    while (*pres) {
      print_pos(*pres);
      ++found;
      free(*pres);
      pres++;
    }

    free(res);
  }
  assert_int_equal(found, matches);

  DESTROY(s);
}

/**
 * Tests filtering within a single batch.
 *
 * @param arg whatever cmocka passes here
 */
void single_batch(void **arg) {
  const char *fullpath = make_file("abc def");
  const char *globs[] = {
    "abc def",
    "abc",
    "def",
    NULL
  };
  extractor_c *ex = make_extractor(globs);
  extractor_c *ex2 = make_extractor(globs);
  ex2->set_flags(ex2, E_NO_ENCLOSED_OCCURRENCES);

  test_match(ex, 10, fullpath, 3);
  test_match(ex2, 10, fullpath, 1);
}

/**
 * Tests filtering within multiple batches.
 *
 * @param arg whatever cmocka passes here
 */
void multi_batch(void **arg) {
  const char *fullpath = make_file("abc def ghi jkl");
  const char *globs[] = {
    "abc def",
    "abc",
    "def",
    "def ghi",
    "ghi",
    "jkl",
    NULL
  };
  extractor_c *ex = make_extractor(globs);
  extractor_c *ex2 = make_extractor(globs);
  ex2->set_flags(ex2, E_NO_ENCLOSED_OCCURRENCES);

  test_match(ex, 3, fullpath, 6);
  test_match(ex2, 3, fullpath, 3);
}

/**
 * Tests filtering with batch of size 1.
 *
 * @param arg whatever cmocka passes here
 */
void small_batch(void **arg) {
  const char *fullpath = make_file("abc def ghi jkl");
  const char *globs[] = {
    "abc def",
    "abc",
    "def",
    "def ghi",
    "ghi",
    "jkl",
    NULL
  };
  extractor_c *ex = make_extractor(globs);
  extractor_c *ex2 = make_extractor(globs);
  ex2->set_flags(ex2, E_NO_ENCLOSED_OCCURRENCES);

  test_match(ex, 1, fullpath, 6);
  test_match(ex2, 1, fullpath, 3);
}

void add_regex(extractor_c *extractor, const char *regex) {
  regex_t * re = regex_compile(regex, "my_regex_fn", regex);
  assert_true(re->state);
  regex_module_c * g_module = regex_module_c_new(regex, TEST_DIR);
  g_module->add_regex(g_module, re);
  assert_true(g_module->build(g_module));
  g_module->load(g_module, extractor);
}

/**
 * Tests whether identical occurrences with different labels are kept.
 *
 * @param arg whatever cmocka passes here
 */
void identical_ranges(void **arg) {
  const char *fullpath = make_file("abc");
  const char *globs[] = {
    "abc",
    NULL
  };
  extractor_c *ex = make_extractor(globs);
  add_regex(ex, globs[0]);
  ex->set_flags(ex, E_NO_ENCLOSED_OCCURRENCES);

  test_match(ex, 1, fullpath, 2);
}

/**
 * Destroys created extractors and deletes created files.
 */
void cleanup(void) {
  printf("Destroying %ld extractors.\n", extractors_count);
  extractor_c **ptr = extractors;
  while (*ptr != NULL) {
    DESTROY(*ptr);
    ++ptr;
  }

  #define Popen system
  const char *cmd = "rm -rf " TEST_DIR;
  puts(cmd);
  Popen(cmd);
}

/**
 * Tests enclosed occurrences.
 *
 * An occurrence A is enclosed in occurrence B if 
 * A.start >= B.start and A.end <= B.end.
 * 
 * For example:
 *   A: {start: 3, end: 6}
 *   B: {start: 1, end: 9}
 *   C: {start: 1, end: 7}
 *   D: {start: 3, end: 9}
 * Or as ASCII-art:
 *   A:  |--|
 *   B: |-------|
 *   C: |-----|
 *   D:  |------|
 *
 * A, C and D are all enclosed in B.
 *
 * @param argc the number of arguments
 * @param argv the arguments
 *
 * @return whatever cmocka returns
 */
int main(int argc, char *argv[]) {
  init();

  const struct CMUnitTest tests[] = {
    cmocka_unit_test(single_batch),
    cmocka_unit_test(multi_batch),
    cmocka_unit_test(small_batch),
    cmocka_unit_test(identical_ranges)
  };

  atexit(cleanup);

  return cmocka_run_group_tests(tests, NULL, NULL);
}

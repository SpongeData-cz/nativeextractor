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

#include <nativeextractor/extractor.h>

extractor_c * g_ex = NULL;

void init() {
  g_ex = extractor_c_new(1, NULL);
}

void constructor(void **state) {
  extractor_c * te = g_ex;
  assert_non_null(te);
  //assert_non_null(te->stream);
  assert_non_null(te->dlsymbols);
  assert_null(*(te->dlsymbols));
  assert_int_equal(te->miners_count, 0);
}

void multiple_open() {
  extractor_c * te = g_ex;
  stream_file_c * s = stream_file_c_new("tests/fixtures/test.txt");
  int ret = te->set_stream(te, (stream_c*)s);
  assert_true(ret);
  assert_memory_equal(te->get_last_error(te), "", 1);
  assert_int_equal(te->stream->state_flags & STREAM_BOF, STREAM_BOF);

  DESTROY(s);
  te->unset_stream(te);

  s = stream_file_c_new("tests/fixtures/test.txt");

  ret = te->set_stream(te, (stream_c*)s);
  assert_true(ret);
  assert_memory_equal(te->get_last_error(te), "", 1);
  assert_int_equal(te->stream->state_flags & STREAM_BOF, STREAM_BOF);

  DESTROY(s);
}

void null_file(void **state) {
  extractor_c * te = g_ex;
  stream_file_c * s = stream_file_c_new("tests/fixtures/null.txt");
  int ret = te->set_stream(te, (stream_c*)s);
  puts(te->get_last_error(te));
  assert_true(ret);
  DESTROY(s);
}

void mining(void **state) {
  extractor_c *te = g_ex;
  assert_true(
    te->add_miner_so(
      te,
      "./build/debug/lib/web_entities.so",
      "match_url",
      NULL
    )
  );
  assert_true(
    te->add_miner_so(
      te,
      "./build/debug/lib/web_entities.so",
      "match_email",
      NULL
    )
  );

  stream_file_c * str = stream_file_c_new("./tests/fixtures/test_web.txt");
  assert_true(te->set_stream(te, (stream_c*)str));
  assert_true(!((te->stream->state_flags) & STREAM_EOF));

  const char *s = "name@domain.com";
  occurrence_t ** res = te->next(te, strlen(s));
  assert_non_null(res);
  assert_string_equal((*res)->str, s);
  free(*res);
  free(res);
  DESTROY(str);
  te->unset_stream(te);
}

void mining_with_params(void **state) {
  extractor_c *te = g_ex;
  assert_true(
    te->add_miner_so(
      te,
      "./build/debug/lib/glob_entities.so",
      "match_glob",
      "*"
    )
  );

  stream_file_c * str = stream_file_c_new("./tests/fixtures/test_glob_patterns.txt");

  assert_true(te->set_stream(te, (stream_c*)str));
  assert_true(!((te->stream->state_flags) & STREAM_EOF));

  const char *s = "abc";
  occurrence_t ** res = te->next(te, strlen(s));
  assert_non_null(res);
  assert_string_equal((*res)->label, "Glob");
  free(*res);
  free(res);
  DESTROY(str);
}

void buffer_mining(void **steak) {
  miner_c **m = malloc(sizeof(miner_c *) * 1);
  m[0] = (miner_c *) NULL;
  extractor_c *ex = extractor_c_new(1, m);

  ex->add_miner_so(ex, "./build/debug/lib/web_entities.so", "match_url", NULL);

  const char *url = "http://www.google.com";

  const char *texts[] = {
    url,
    url,
    url,
    NULL
  };

  size_t texts_len = (sizeof texts / sizeof *texts) - 1;
  size_t found_texts = 0;

  const char **x = texts;
  while (*x != NULL) {
    stream_buffer_c *bf = stream_buffer_c_new(*x, strlen(*x));
    assert_false(bf->stream.state_flags & STREAM_FAILED);

    ex->set_stream(ex, bf);

    while (!((ex->stream->state_flags) & STREAM_EOF)) {
      occurrence_t **res = ex->next(ex, 1000);
      occurrence_t **pres = res;

      while (*pres) {
        assert_string_equal((*pres)->str, url);
        found_texts++;
        free(*pres);
        pres++;
      }

      free(res);
    }

    ex->unset_stream(ex);
    DESTROY(bf);
    ++x;
  }

  DESTROY(ex);

  assert_int_equal(texts_len, found_texts);
}

void meta_info(void **state) {
  miner_c **m = malloc(sizeof(miner_c *) * 1);
  m[0] = (miner_c *) NULL;
  extractor_c *ex = extractor_c_new(1, m);

  ex->add_miner_so(ex, "./build/debug/lib/web_entities.so", "match_url", NULL);

  const dl_symbol_t *dlsym = ex->dlsymbols[0];
  assert_string_equal(dlsym->meta[0], "match_url");
  assert_string_equal(dlsym->meta[1], "URL");

  DESTROY(ex);
}

int main(int argc, char *argv[]) {
  init();

  const struct CMUnitTest tests[] = {
    cmocka_unit_test(constructor),
    cmocka_unit_test(multiple_open),
    cmocka_unit_test(null_file),
    //cmocka_unit_test(mining), // Nonfree only
    cmocka_unit_test(mining_with_params),
    //cmocka_unit_test(buffer_mining), // Nonfree only
    //cmocka_unit_test(meta_info) // Nonfree only
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}

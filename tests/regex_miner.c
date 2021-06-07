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
#include <nativeextractor/regex_generator.h>

#define FIXTURE_0 "./tests/fixtures/regex_generator/fixture_0.txt"

regex_module_c * g_module = NULL;
extractor_c * g_e = NULL;

void fill_module() {
  regex_t *re_email = regex_compile("[^@ \\t\\r\\n]+@[^@ \\t\\r\\n]+\\.[^@ \\t\\r\\n]+", "simple_email", "EMAIL");

  // regex_t * re_par = regex_compile("[+]?^[-(]\[\])]$", "parse_par", "PAR");

  assert_true( re_email->state );
  assert_null( re_email->errors );

  regex_t * re_tel = regex_compile("[+]?[(]?[0-9]{3}[)]?[-\\s.]?[0-9]{3}[-\\s.]?[0-9]{4,6}", "simple_tel", "TEL_NO");

  assert_true( re_tel->state );
  assert_null( re_tel->errors );

  g_module = regex_module_c_new("testative", NULL);
  assert_true( g_module != NULL );

  g_module->add_regex( g_module, re_email );
  assert_true( g_module->state );
  assert_null( g_module->errors );

  g_module->add_regex( g_module, re_tel );
  assert_true( g_module->state );
  assert_null( g_module->errors );
}

void build_module() {
  assert_true( g_module->build(g_module) );
  assert_null( g_module->errors );
}

void load_module() {
  miner_c ** miners = calloc(1, sizeof(miner_c*));
  g_e = extractor_c_new(-1, miners);

  assert_true( g_module->load(g_module, g_e) );
  assert_true( g_module->state );
  assert_null( g_module->errors );
}

void extract() {
  stream_file_c * strf = stream_file_c_new(FIXTURE_0);
  g_e->set_stream(g_e, (stream_c*)strf);

  uint32_t count = 0;

  while (!((g_e->stream->state_flags) & STREAM_EOF)) {
    occurrence_t ** res = g_e->next(g_e, 10000000); // 10000000
    occurrence_t ** pres = res;

    while (*pres) {
      print_pos(*pres);
      free(*pres);
      ++pres;
      ++count;
    }

    free(res);
  }
  assert_int_equal(count, 2);
}

void cleanup() {
  DESTROY((stream_file_c*)g_e->stream);
  g_e->unset_stream(g_e);
  g_module->destroy(g_module);
  g_e->destroy(g_e);
  free(g_e);
}

int main(int argc, char **argv)
{
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(fill_module),
    cmocka_unit_test(build_module),
    cmocka_unit_test(load_module),
    cmocka_unit_test(load_module),
    cmocka_unit_test(extract),
    cmocka_unit_test(cleanup)
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
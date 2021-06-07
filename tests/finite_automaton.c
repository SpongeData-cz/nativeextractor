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

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>

#include <nativeextractor/finite_automaton.h>

void wiki() {
  printf("NFA:\n");
  fa_t *fa = fa_create();

  size_t q1 = fa_add_node(fa);
  fa_get_node(fa, q1)->is_starting = true;

  size_t q2 = fa_add_node(fa);

  size_t q3 = fa_add_node(fa);
  fa_get_node(fa, q3)->is_final = true;

  size_t q4 = fa_add_node(fa);
  fa_get_node(fa, q4)->is_final = true;

  fa_add_edge(fa, q1, "0", q2);
  fa_add_edge(fa, q1, NULL, q3);

  fa_add_edge(fa, q2, "1", q2);
  fa_add_edge(fa, q2, "1", q4);

  fa_add_edge(fa, q3, "0", q4);
  fa_add_edge(fa, q3, NULL, q2);

  fa_add_edge(fa, q4, "0", q3);

  fa_print(fa);

  printf("\nDFA:\n");
  fa_t *powerset = fa_create_powerset(fa);

  fa_destroy(fa);
  fa_destroy(powerset);

  // TODO: Add tests
  assert_true(true);
}

int main(int argc, const char *argv) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(wiki)
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}

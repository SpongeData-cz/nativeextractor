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

#include <nativeextractor/patricia.h>

#define WORDS_COUNT 10000
#define PATTY_PATH  "./tests/fixtures/test.patty"

patricia_c * g_pat = NULL;
char ** rand_strings = NULL;

patricia_c * init() {
  g_pat = patricia_c_create(NULL);
  rand_strings = calloc(WORDS_COUNT+1, sizeof(char*));
  srand(0);
}

char * rand_str(unsigned * len) {
  *len = 1+(rand() % 16);
  char * out = malloc( sizeof(char)*(*len +1) );
  out[*len] = '\0';

  for(unsigned i = 0 ; i < *len ; i++ ){
    out[i] = 'A' + (rand() % ('z' - '~'));
  }

  return out;
}

void rand_fill() {
  for( unsigned w = 0 ; w < WORDS_COUNT ; w++ ){
    unsigned len = 0;
    rand_strings[w] = rand_str(&len);

    assert_non_null( rand_strings[w] );
    g_pat->insert(g_pat, rand_strings[w], 0, len);

    uint32_t ret = g_pat->search(g_pat, rand_strings[w], len);

    assert_true(ret == len);
  }
}

void rand_check() {
  char ** p_rs = rand_strings;
  while( *p_rs ){
    uint32_t str_len = strlen(*p_rs);
    uint32_t ret = g_pat->search(g_pat, *p_rs, str_len);
    if( ret < 1 ){
      printf("ERROR: `%s` not found\n", *p_rs);
    }
    assert_true(ret == str_len);
    p_rs++;
  }
}

void rand_check_pfx() {
  for( unsigned w = 0 ; w < WORDS_COUNT ; w++ ){
    unsigned len = strlen(rand_strings[w]);
    unsigned pfx_len = len >> 1;
    pfx_len = (pfx_len < 1 ? 1 : pfx_len);

    uint32_t ret = g_pat->search(g_pat, rand_strings[w], pfx_len);
    if( !(ret == pfx_len) ){
      printf("%u != %u\n", pfx_len, ret);
      printf("%s\n", rand_strings[w]);
    }
    assert_true( ret == pfx_len );
    if( ret == 0 ){
      for( unsigned wn = 0 ; wn < WORDS_COUNT ; wn++ ){
        if( wn == w ){ continue; }
        if( pfx_len != strlen(rand_strings[wn]) ){ continue; } /* can't be equal */
        assert_memory_not_equal(rand_strings[w], rand_strings[wn], pfx_len );
      }
    }
  }
}

void node_check_sorting(patricia_node_t * n, unsigned flags, void * context) {
  patricia_c * pc = (patricia_c*)context;

  if( n->edge_count <= 1 ){
    return;
  }

  for( unsigned e = 1 ; e < n->edge_count ; ++e ) {
    patricia_edge_t e1 = PATRICIA_C_EDGES(n)[e-1];
    patricia_edge_t e2 = PATRICIA_C_EDGES(n)[e];
    uint32_t s1 = unicode_to_int((char*)pc->lookup_str + e1.str_start);
    uint32_t s2 = unicode_to_int((char*)pc->lookup_str + e2.str_start);
    int cmp = CMP(s1, s2);
    assert_true( cmp <= 0 );
  }
}

void check_sorting() {
  g_pat->traverse(g_pat, TRAVERSE_NODE_PREODER, node_check_sorting, NULL, g_pat);
}

unsigned rand_unicode_char( char * str ){
  unsigned max_bytes = (rand() % 3)+2; // 2-4 bytes only
  unsigned char masks[4] = {0b00000000, 0b11000000, 0b11100000, 0b11110000};
  unsigned char mask = masks[max_bytes-1];
  str[0] = mask;

  for(uint64_t i = 1 ; i < max_bytes ; ++i ){
    str[i] = (0b00111111 & rand() % 256) | 0b10000000;
  }

  return max_bytes;
}

char * rand_unicode(unsigned * len) {
  *len = 1+(rand() % 16);
  char * out = calloc((4*(*len)) +1, sizeof(char));
  unsigned offset = 0;

  for(unsigned i = 0 ; i < *len ; i++ ){
    char * pout = out + offset;
    offset += rand_unicode_char( pout );
  }

  *len = offset;

  return out;
}

void rand_unicode_fill() {
  rand_strings = realloc( rand_strings, (2*WORDS_COUNT+1)*sizeof(char *) );
  rand_strings[2*WORDS_COUNT] = NULL;

  for( unsigned wu = WORDS_COUNT; wu < 2*WORDS_COUNT; ++wu ) {
    unsigned len;
    rand_strings[wu] = rand_unicode(&len);

    g_pat->insert(g_pat, rand_strings[wu], 0, len);
    uint32_t ret = g_pat->search(g_pat, rand_strings[wu], len);
    if( ret == 0 ){
      printf("FAILED: %s\n", rand_strings[wu]);
    }

    assert_true( ret == len );
  }
}

void insert_data(){
  for( unsigned wu = WORDS_COUNT; wu < 2*WORDS_COUNT; ++wu ) {
    size_t str_len = strlen(rand_strings[wu])+1;
    void * ret = g_pat->set(g_pat, rand_strings[wu], 0, str_len, rand_strings[wu], str_len);

    assert_false( strcmp(ret, rand_strings[wu]) );
    assert_non_null( g_pat->get(g_pat, rand_strings[wu]) );
    assert_false( strcmp( g_pat->get(g_pat, rand_strings[wu]), rand_strings[wu]) );
  }
}

// void one_item_tree(){
//   patricia_c * pat = patricia_c_create(NULL);
//   pat->set(pat, "HELLOWORLD!", 0, strlen("HELLOWORLD!")+1, "GUMOID!", strlen("GUMOID!")+1 );
//   assert_non_null( pat->get(pat, "HELLOWORLD!") );
//   pat->save(pat, "./tests/fixtures/gumoid.patty");

//   pat = patricia_c_from_file("./tests/fixtures/gumoid.patty");
//   char * str = pat->get(pat, "HELLOWORLD!");
//   assert_non_null( str );
// }

void check_data(){
  for( unsigned wu = WORDS_COUNT; wu < 2*WORDS_COUNT; ++wu ) {
    size_t str_len = strlen(rand_strings[wu]);
    char * str = (char*)g_pat->get(g_pat, rand_strings[wu]);

    assert_true( str );
    assert_true( strcmp( g_pat->get(g_pat, rand_strings[wu]), rand_strings[wu]) == 0);
  }
}

void check_multichar_edges() {
  /*
   * The tree below has this structure:
   *
   * a
   *  a
   *   a
   *    a\0
   *    b\0
   *    c\0
   *    d\0
   *    e\0
   *    fa       <-- edge with multiple characters
   *      a\0
   *      b\0
   *      c\0
   *      d\0
   *      e\0
   *      f\0
   *      g\0
   *      h\0
   *    g\0
   *   b\0
   *  b
   *   a\0
   *   b\0
   *
   * The bug being tested here only checked the first character of the edge 
   * and continued if it matched, regardless of other characters in the edge. 
   * This could lead to a terminal node and ultimately return a matched prefix 
   * that is not present in the trie.
   */
  char *words[] = {
    "aaaa",
    "aaab",
    "aaac",
    "aaad",
    "aaae",
    "aaafaa",
    "aaafab",
    "aaafac",
    "aaafad",
    "aaafae",
    "aaafaf",
    "aaafag",
    "aaafah",
    "aaag",
    "aab",
    "aba",
    "abb",
    NULL
  };
  patricia_c *p = patricia_c_create(NULL);
  for (unsigned i = 0; words[i] != NULL; ++i) {
    char *value = words[i];
    p->insert(p, value, 0, strlen(value));
  }

  char *search[] = {
    "aaafah",
    "aaafb",
    NULL
  };
  const bool terminal[] = {
    true,
    false
  };
  const unsigned matches[] = {
    6,
    4
  };

  for (unsigned i = 0; search[i] != NULL; ++i) {
    char *value = search[i];
    patricia_node_t *node = NULL;
    uint32_t match = p->search_ext(p, value, strlen(value), &node);
    assert_int_equal(match, matches[i]);
    assert_int_equal(node->is_terminal, terminal[i]);
  }
}

void add_empty_str() {
  g_pat->insert(g_pat, "", 0, 0);
  assert_true( g_pat->search(g_pat, "", 0) > 0 );
}

void patty_save_reopen() {
  const char * patty_path = PATTY_PATH;
  assert_true( g_pat->save(g_pat, patty_path) );

  size_t old_lookup_sz = g_pat->data_lookup_sz;

  g_pat->destroy(g_pat);
  g_pat = NULL;

  g_pat = patricia_c_from_file(patty_path);

  assert_int_equal(g_pat->data_lookup_sz, old_lookup_sz);
  assert_non_null( g_pat );
}

void cleanup() {
  g_pat->destroy(g_pat);
  remove(PATTY_PATH);
  char ** pw = rand_strings;
  while( *pw ){
    free( *pw );
    pw++;
  }

  free( rand_strings );
}

int main(int argc, char *argv[]) {
  init();

  const struct CMUnitTest tests[] = {
    cmocka_unit_test(rand_fill),
    cmocka_unit_test(rand_check),
    cmocka_unit_test(rand_check_pfx),
    cmocka_unit_test(check_sorting),
    cmocka_unit_test(rand_unicode_fill),
    cmocka_unit_test(rand_check),
    cmocka_unit_test(add_empty_str),
    cmocka_unit_test(insert_data),
    // cmocka_unit_test(one_item_tree),
    cmocka_unit_test(patty_save_reopen),
    cmocka_unit_test(rand_check),
    cmocka_unit_test(check_data),
    cmocka_unit_test(check_multichar_edges),
    cmocka_unit_test(cleanup)
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}

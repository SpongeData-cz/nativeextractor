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

#include <nativeextractor/patricia.h>
#include <nativeextractor/csv_parser.h>
#include <string.h>

static inline void _print_edge(patricia_edge_t* edge, uint32_t indent, patricia_c * tree);

static inline void _print(patricia_node_t* self, uint32_t indent, patricia_c * tree);

static inline uint32_t _find_matching_part(patricia_edge_t* edge, const char * lookup_csv, char* str, uint64_t str_start, uint32_t str_len) {
  uint32_t i = 0;
  uint32_t j = 0;

  char* edge_str = (char*)lookup_csv + edge->str_start;
  char* match_str = str + str_start;

  while (true) {
    if (i >= edge->str_len || j >= str_len) break;
    uint32_t s1c = unicode_to_int(&edge_str[i]);
    uint32_t s2c = unicode_to_int(&match_str[j]);
    if (CMP(s1c, s2c) != 0) break;
    i += unicode_getbytesize(&edge_str[i]);
    j += unicode_getbytesize(&match_str[j]);
  }

  return i;
}

static inline int _str_compare(const char * s1, uint32_t l1, const char * s2, uint32_t l2){
  uint32_t i = 0;
  uint32_t j = 0;
  uint32_t len = MIN(l1, l2);
  for (uint32_t l = 0; l < len; ++l) {
    uint32_t s1c = unicode_to_int((char*)&s1[i]);
    uint32_t s2c = unicode_to_int((char*)&s2[j]);
    int cmp = CMP(s1c, s2c);
    if (cmp != 0) return cmp;
    i += unicode_getbytesize((char*)&s1[i]);
    j += unicode_getbytesize((char*)&s2[j]);
  }
  return CMP(l1, l2);
}

static inline int _edge_compare(patricia_edge_t* e1, patricia_edge_t* e2, const char * lookup_csv) {
  return _str_compare( lookup_csv+e1->str_start, e1->str_len, lookup_csv+e2->str_start, e2->str_len);
}

static inline patricia_node_t * _insert_edge(patricia_node_t* node, patricia_edge_t* edge, const char * csv_lookup) {
  ++node->edge_count;
  node = realloc(node, sizeof(patricia_node_t) + (sizeof(patricia_edge_t) * (node->edge_count)));

  uint32_t i = node->edge_count - 1;
  for (/**/; i > 0; --i) {
    if (_edge_compare(edge, &(PATRICIA_C_EDGES(node)[i - 1]), csv_lookup) >= 0) break;
    PATRICIA_C_EDGES(node)[i] = PATRICIA_C_EDGES(node)[i - 1];
  }
  PATRICIA_C_EDGES(node)[i] = *edge;

  return node;
}

patricia_node_t * patricia_node_t_create(patricia_node_t * node){
  patricia_node_t * out = ALLOC(patricia_node_t);
  if( node )
    memcpy(out, node, sizeof(patricia_node_t));
  else
    memset(out, 0, sizeof(patricia_node_t));

  return out;
}

static inline patricia_node_t* _split_edge(patricia_edge_t* edge, uint32_t at, const char * lookup_csv) {
  patricia_edge_t edge_new;

  edge_new.str_start = edge->str_start + at;
  edge_new.str_len = edge->str_len - at;
  edge_new.next = edge->next;

  patricia_node_t* p_new = patricia_node_t_create(NULL);
  p_new = _insert_edge(p_new, &edge_new, lookup_csv);

  edge->next = p_new;
  edge->str_len = at;

  return p_new;
}

static inline patricia_node_t* _insert(patricia_node_t* node, char* str, uint64_t str_start, uint32_t str_len, const char* lookup_csv, patricia_node_t** addr) {
  if (str_len <= 0) {
    node->is_terminal = true;
    return node;
  }

  for (uint32_t i = 0; i < node->edge_count; ++i) {
    patricia_edge_t* edge = &(PATRICIA_C_EDGES(node)[i]);
    uint32_t match_till = _find_matching_part(edge, lookup_csv, str, str_start, str_len);
    if (match_till == 0) continue;
    node = (match_till < edge->str_len)
      ? _split_edge(edge, match_till, lookup_csv)
      : edge->next;
    str_start += match_till;
    str_len -= match_till;
    addr = &(edge->next);
    return _insert(node, str, str_start, str_len, lookup_csv, addr);
  }

  patricia_edge_t edge_new;
  edge_new.str_start = str_start;
  edge_new.str_len = str_len;
  edge_new.next = patricia_node_t_create(NULL);
  edge_new.next->is_terminal = true;
  *addr = _insert_edge(node, &edge_new, lookup_csv);
  return edge_new.next;
}

patricia_node_t* patricia_c_insert(patricia_c* self, char* str, uint64_t str_start, uint32_t str_len) {
  self->lookup_str = str;
  return _insert(self->root, str, str_start, str_len, self->lookup_str, &(self->root));
}

patricia_node_t* patricia_c_insert_no_lookup(patricia_c * self, char* str, uint64_t str_start, uint32_t str_len){
  uint64_t old_sz = self->lookup_sz;
  char * old_end = self->lookup_str + self->lookup_sz;

  self->lookup_sz += str_len;

  if( !self->lookup_str ) {
    self->lookup_str = malloc( str_len );
    old_end = self->lookup_str;
  } else {
    self->lookup_str = realloc( self->lookup_str, self->lookup_sz );
    old_end = self->lookup_str + old_sz;
  }

  memcpy(old_end, str, str_len);

  return patricia_c_insert(self, self->lookup_str, old_sz, str_len);
}

static inline int64_t find_log(patricia_node_t * node, char * str, uint32_t str_len, const char * csv_lookup) {
  int64_t lmin = 0;
  int64_t rmax = node->edge_count - 1;
  int64_t pivot = node->edge_count >> 1;

  patricia_edge_t * edge;

  while (rmax >= lmin) {
    // printf("edge_count: %u, pivot: %ld\n", node->edge_count, pivot);

    edge = &(PATRICIA_C_EDGES(node)[pivot]);

    uint32_t ec = unicode_to_int((char*)csv_lookup + edge->str_start);
    uint32_t sc = unicode_to_int(str);
    int cmp = CMP(ec, sc);

    switch (cmp) {
      case -1:
        lmin = pivot+1;
        break;
      case 0:
        return pivot;
        break;
      case 1:
        rmax = pivot-1;
        break;
    }

    pivot = (lmin + rmax) >> 1;
  }

  return -1;
}

static inline int64_t find_linear(patricia_node_t * node, char * str, uint32_t str_len, const char * csv_lookup){
  for (uint32_t i = 0; i < node->edge_count; ++i) {
    patricia_edge_t* edge = &(PATRICIA_C_EDGES(node)[i]);
    if (str_len < edge->str_len) {
      continue;
    }
    uint32_t ec = unicode_to_int((char*)csv_lookup + edge->str_start);
    uint32_t sc = unicode_to_int(str);
    int cmp = CMP(ec, sc);
    if (cmp == 0) {
      return (int64_t)i;
    }
  }
  return -1;
}

static inline int64_t find_fragment(patricia_node_t * node, char * str, uint32_t str_len, const char * csv_lookup){
  for (uint32_t i = 0; i < node->edge_count; ++i) {
    patricia_edge_t* edge = &(PATRICIA_C_EDGES(node)[i]);

    uint32_t ec = unicode_to_int((char*)csv_lookup + edge->str_start);
    uint32_t sc = unicode_to_int(str);
    int cmp = CMP(ec, sc);
    if (cmp == 0) {
      return (int64_t)i;
    }
  }

  return -1;
}

/**
 * @param self
 * @param node
 * @param str
 * @param str_start  The starting position of the string within str.
 * @param str_offset The current offset from the starting position. Used to
 *                   determine partial matches.
 * @param str_len    String length in bytes (without \0).
 * @param lookup_csv
 */
static inline uint32_t _search(
  patricia_c* self, patricia_node_t** node,
  char* str, uint64_t str_start, uint64_t str_offset, uint32_t str_len,
  const char* lookup_csv)
{
  int64_t i = ((*node)->edge_count <= 5)
    ? find_linear(*node, str + str_start + str_offset, str_len - str_offset, lookup_csv)
    : find_log(*node, str + str_start + str_offset, str_len - str_offset, lookup_csv);

  if (i < 0) {
    i = find_fragment(*node, str + str_start + str_offset, str_len - str_offset, lookup_csv);
    if( i < 0 ){ /* nothing found*/
      return str_offset;
    }

    uint32_t common_len = _find_matching_part(
      &(PATRICIA_C_EDGES(*node)[i]), lookup_csv, str, str_start + str_offset, str_len - str_offset);
    return str_offset + common_len;
  }

  patricia_edge_t* edge = &(PATRICIA_C_EDGES(*node)[i]);
  uint32_t match_till = _find_matching_part(
    edge, lookup_csv, str, str_start + str_offset, str_len - str_offset);

  *node = PATRICIA_C_NEXT_NODE(edge, self);
  if (match_till + str_offset == str_len) {
    return str_len;
    // return (*node)->is_terminal ? 1.0 : 0.0;
  }

  return _search(self, node, str, str_start, str_offset + match_till, str_len, lookup_csv);
}

uint32_t patricia_c_search_ext(patricia_c* self, char* str, uint32_t str_len, patricia_node_t** node /* out */) {
  *node = self->root;

  if (str_len == 0) {
    return self->root->is_terminal;
  }

  return _search(self, node, str, 0, 0, str_len, self->lookup_str);
}

uint32_t patricia_c_search(patricia_c* self, char* str, uint32_t str_len) {
  if (str_len == 0) {
    return self->root->is_terminal;
  }

  patricia_node_t * root = self->root;

  return _search(self, &root, str, 0, 0, str_len, self->lookup_str);
}

static void _print_edge(patricia_edge_t* edge, uint32_t indent, patricia_c * tree) {
  printf("%*s", indent, "");
  char* str = (char*)tree->lookup_str + edge->str_start;
  char* end = str + edge->str_len;
  uint32_t len = 0;
  while (str < end) {
    print_unicode(str);
    str += unicode_getbytesize(str);
    ++len;
  }

  patricia_node_t * next_node = PATRICIA_C_NEXT_NODE(edge, tree);
  if (next_node->is_terminal) {
    printf("\\0");
  }
  putchar('\n');
  _print(next_node, indent + len, tree);
}

static inline void _print(patricia_node_t* self, uint32_t indent, patricia_c * tree) {
  for (uint32_t i = 0; i < self->edge_count; ++i) {
    _print_edge(&(PATRICIA_C_EDGES(self)[i]), indent, tree);
  }
}

void patricia_c_print(patricia_c* self) {
  _print(self->root, 0, self);
}

void patricia_node_t_destroy(patricia_node_t * node);

static inline void patricia_edge_t_destroy(patricia_edge_t* self) {
  patricia_node_t_destroy(self->next);
}

void patricia_node_t_destroy(patricia_node_t * node){
  for (uint32_t i = 0; i < node->edge_count; ++i) {
    patricia_edge_t_destroy(&(PATRICIA_C_EDGES(node)[i]));
  }

  free(node);
}

void patricia_c_destroy(patricia_c* self) {
  if( self->mmap_binary ){
    DESTROY(self->mmap_binary);
  }
  else{
    patricia_node_t_destroy(self->root);

    if (self->csv != NULL) {
      //DESTROY(self->csv);
      self->csv = NULL; /* Decrement refcount */
    } else {
      free( self->lookup_str );
    }
  }
}

patricia_c* patricia_c_create(stream_c * stream) {
  patricia_c* p = ALLOC(patricia_c);
  patricia_c_init(p, stream);
  return p;
}

patricia_c* patricia_c_create_from_stream(stream_c * stream) {
  patricia_c* p = patricia_c_create(NULL);
  csv_parser_c* csv_parser = csv_parser_create(stream);
  csv_batch_c* batch = csv_parser_parse(csv_parser, 0);

  while (csv_batch_has_next(batch)) {
    char* value = csv_batch_get_next(batch);
    if (!CSV_IS_NEWLINE(value)) {
      p->insert(p, value, 0, strlen(value));
    }
  }

  DESTROY(batch);
  DESTROY(csv_parser);
  return p;

  // stream_c* csv = stream;

  // if (!csv) {
  //   DESTROY(csv);
  //   return NULL;
  // }

  // patricia_c* p = patricia_c_create(csv);

  // char* start = csv->start;
  // p->lookup_str = start;

  // while (!(csv->state_flags & STREAM_EOF)) {
  //   char* chr = csv->next_char(csv);
  //   if (*chr == '\n' && csv->pos > start) {
  //     uint64_t str_start = (uint64_t)(start - csv->start);
  //     uint32_t str_len = (uint32_t)(csv->pos - start - 1);
  //     // printf(", ");
  //     p->insert(p, csv->start, str_start, str_len);
  //     start = csv->pos;
  //   }
  // }
  // // putchar('\n');

  // p->csv = csv;
  // return p;
}

static void patricia_c_traverse_impl( patricia_node_t * node, unsigned flags,
  void (*n_traversal_fn)(patricia_node_t * n, unsigned flags, void * context),
  void (*e_traversal_fn)(patricia_edge_t e, unsigned flags, void * context),
  void * context) {
    if( flags & TRAVERSE_NODE_PREODER ){
      if( n_traversal_fn )
        n_traversal_fn(node, TRAVERSE_NODE_PREODER, context);
    }

    for( unsigned ec = 0; ec < node->edge_count; ec++ ){
      patricia_edge_t edge = PATRICIA_C_EDGES(node)[ec];
      if( flags & TRAVERSE_EDGE_PREODER ){
        if( e_traversal_fn )
          e_traversal_fn( edge, TRAVERSE_EDGE_PREODER, context );
      }

      patricia_c_traverse_impl(edge.next, flags, n_traversal_fn, e_traversal_fn, context);

      if( flags & TRAVERSE_EDGE_POSTORDER ){
        if( e_traversal_fn )
          e_traversal_fn( edge, TRAVERSE_EDGE_POSTORDER, context );
      }
    }

    if( flags & TRAVERSE_NODE_POSTORDER ){
      if( n_traversal_fn )
        n_traversal_fn(node, TRAVERSE_NODE_POSTORDER, context);
    }
}

void patricia_c_traverse(patricia_c * self, unsigned flags,
  void (*n_traversal_fn)(patricia_node_t * n, unsigned flags, void * context),
  void (*e_traversal_fn)(patricia_edge_t e, unsigned flags, void * context),
  void * context){
    patricia_c_traverse_impl(self->root, flags, n_traversal_fn, e_traversal_fn, context);
}

typedef struct patricia_save_context_t{
  patricia_header ph;
  FILE * fd;
  uint64_t offset;
  patricia_node_t * ctx_node;
  uint64_t return_node_offset;
  bool error;
} patricia_save_context_t;

uint64_t patricia_c_save_impl(patricia_node_t * node, patricia_save_context_t * psc, patricia_c * tree){
  uint64_t pnode_sz = sizeof(patricia_node_t) + (sizeof(patricia_edge_t)*node->edge_count);
  patricia_node_t * node_record = calloc(1, pnode_sz);
  node_record->edge_count = node->edge_count;
  node_record->is_terminal = node->is_terminal;
  node_record->user_data_offset = node->user_data_offset;

  for( unsigned ec = 0; ec < node->edge_count; ec++ ){
    patricia_edge_t edge = PATRICIA_C_EDGES(node)[ec];
    patricia_node_t * next_node = PATRICIA_C_NEXT_NODE(&edge, tree);
    uint64_t offset = patricia_c_save_impl(next_node, psc, tree);
    patricia_binary_edge_t * red = (patricia_binary_edge_t*)(&(PATRICIA_C_EDGES(node_record)[ec]));

    red->str_len = edge.str_len;
    red->str_start = edge.str_start;
    red->next_offset = offset;
    psc->ph.edges_count++;
  }

  fwrite(node_record, pnode_sz, 1, psc->fd);
  free( node_record );
  psc->ph.nodes_count++;
  uint64_t prev_offset = psc->offset;
  psc->offset += pnode_sz;
  psc->ph.nodes_count++;

  return prev_offset;
}

bool patricia_c_save(patricia_c* self, const char * path){
  FILE * f = fopen(path, "wb+");
  uint64_t offset = 0;

  if( !f ){
    // No such file or directory
    return false;
  }

  if( self->mmap_binary ){
    /* save whole file and end */
    fwrite(self->mmap_binary->stream.start, sizeof(char), self->mmap_binary->stream.fsize, f);

    fclose(f);
    return true;
  }

  patricia_header ph;
  memset( &ph, 0, sizeof(patricia_header) );
  memcpy(&(ph.magic_str), "PATTY", sizeof(char)*5);

  ph.edges_count = 0;
  ph.nodes_count = 0;
  ph.saved_str_size = 0;
  ph.size = sizeof(patricia_header);
  ph.lookup_length = (self->csv ? self->csv->fsize : self->lookup_sz );
  ph.data_lookup_length = self->data_lookup_sz;
  ph.data_lookup_offset = 0;

  fwrite(&ph, sizeof(patricia_header), 1, f);
  offset += sizeof(patricia_header);

  /* write lookup csv */
  fwrite((self->csv ? self->csv->start : self->lookup_str), sizeof(char), ph.lookup_length, f);
  offset += sizeof(char) * ph.lookup_length;

  /* write data lookup */
  if( self->data_lookup ){
    fwrite(self->data_lookup, sizeof(char), ph.data_lookup_length, f);
    ph.data_lookup_offset = offset;
    offset += sizeof(char)*ph.data_lookup_length;
  }

  patricia_save_context_t psc;
  memset(&psc, 0, sizeof(patricia_save_context_t));
  psc.ph = ph;
  psc.fd = f;
  psc.offset = offset;
  psc.ctx_node = NULL;
  psc.return_node_offset = 0;
  psc.error = false;

  uint64_t root_offset = patricia_c_save_impl(self->root, &psc, self);

  psc.ph.root_offset = root_offset;
  psc.ph.size = psc.offset;

  fseek(f, 0, SEEK_SET);
  fwrite(&(psc.ph), sizeof(patricia_header), 1, f);

  fclose(f);

  return true;
}

void * get(struct patricia_c * self, const char * str){
  size_t str_len = strlen(str)+1;
  patricia_node_t * root = self->root;

  uint32_t ret = _search(self, &root, (char*)str, 0, 0, str_len, self->lookup_str);

  if (ret > 0 && root->is_terminal) {
    return self->data_lookup + root->user_data_offset;
  }

  return NULL;
}

void * set_data(patricia_c * self, patricia_node_t * node, void * data, size_t data_sz){
  size_t added_data_offset = self->data_lookup_sz;

  self->data_lookup = realloc(self->data_lookup, self->data_lookup_sz + data_sz);
  memcpy( self->data_lookup + self->data_lookup_sz, data, data_sz );
  self->data_lookup_sz += data_sz;
  node->user_data_offset = added_data_offset;

  return self->data_lookup + added_data_offset;
}

void * set( patricia_c * self, const char * str, uint64_t str_start,
  uint32_t str_len, void * data, size_t data_size) {
  patricia_node_t* node = self->insert(self, (char*)str, str_start, str_len);

  return set_data(self, node, data, data_size);
}

void patricia_c_init(patricia_c* self, stream_c * stream) {
  memset(self, 0, sizeof(patricia_c));

  self->root = patricia_node_t_create(NULL);
  self->csv = stream;

  if( stream ){
    self->lookup_str = self->csv->start;
    self->lookup_sz = self->csv->fsize;
    self->insert = patricia_c_insert;
  }
  else {
    self->insert = patricia_c_insert_no_lookup;
  }

  self->mmap_binary = NULL;
  self->csv = NULL;

  self->search = patricia_c_search;
  self->search_ext = patricia_c_search_ext;
  self->print = patricia_c_print;
  self->destroy = patricia_c_destroy;
  self->save = patricia_c_save;
  self->traverse = patricia_c_traverse;
  self->get = get;
  self->set = set;
  self->set_data = set_data;
}

patricia_c* patricia_c_from_file(const char* fullpath) {
  patricia_header * ph = NULL;
  stream_file_c* file_stream = stream_file_c_new(fullpath);

  if (file_stream->stream.state_flags &= STREAM_FAILED) {
    DESTROY(file_stream);
    return NULL;
  }

  patricia_c* p = patricia_c_create((stream_c *)file_stream);
  /* disable unsupported ops */
  p->insert = NULL;
  p->set = NULL;

  ph = (patricia_header*)file_stream->stream.start;

  p->mmap_binary = file_stream;
  p->lookup_str = file_stream->stream.start + sizeof(patricia_header);
  p->lookup_sz = ph->saved_str_size;

  p->data_lookup = ( ph->data_lookup_length > 0 ? p->mmap_binary->stream.start + ph->data_lookup_offset : NULL);
  p->data_lookup_sz = ph->data_lookup_length;

  patricia_node_t_destroy(p->root);

  p->root = (patricia_node_t*)(file_stream->stream.start + ph->root_offset);
  p->csv = NULL;

  return p;
}

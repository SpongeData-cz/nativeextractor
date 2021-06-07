// Copyright (C) 2021 SpongeData s.r.o.
//
// NativeExtractor is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// NativeExtractor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with NativeExtractor. If not, see <http://www.gnu.org/licenses/>.

#ifndef PATRICIA_H
#define PATRICIA_H

#include <nativeextractor/common.h>
#include <nativeextractor/stream.h>
#include <nativeextractor/unicode.h>

/**
 * Takes from a node list of edges
 */
#define PATRICIA_C_EDGES(node) ((patricia_edge_t*)((patricia_node_t*)node+1))

/**
 * Takes from an edge and patricia_c tree next patricia node
 */
#define PATRICIA_C_NEXT_NODE(edge, tree) (((tree)->mmap_binary ? (patricia_node_t*)((tree)->mmap_binary->stream.start + ((patricia_binary_edge_t*)(edge))->next_offset) : (edge)->next))


/**
 * Traversal flags.
 */
#define TRAVERSE_NODE_PREODER 1
#define TRAVERSE_EDGE_PREODER 1 << 1
#define TRAVERSE_NODE_POSTORDER 1 << 2
#define TRAVERSE_EDGE_POSTORDER 1 << 3

typedef struct patricia_edge_t {
  /** Offset of a value in base string - most commonly csv */
  uint64_t str_start;
  /** Length of that string */
  uint32_t str_len;
  /** Native instance of next node */
  struct patricia_node_t* next;
} patricia_edge_t;

typedef struct patricia_binary_edge_t {
  /** Offset of a value in base string - most commonly csv */
  uint64_t str_start;
  /** Length of that string */
  uint32_t str_len;
  /** Offset to a mmaped file in which is next node stored */
  uint64_t next_offset;
} patricia_binary_edge_t;

typedef struct patricia_node_t {
  /** True to signal an end of a word in the trie. */
  bool is_terminal;

  /** Size of the array of edges. */
  uint32_t edge_count;

  /** User data size */
  size_t user_data_offset;

  /** Array of edges follows... */
} patricia_node_t;

/** Patricia file header for PATTY format */
typedef struct patricia_header {
  /** Magic string - PATTY string is used */
  char magic_str[5];
  /** Nodes count statistics */
  uint64_t nodes_count;
  /** Edges count statistics */
  uint64_t edges_count;
  /** Total string size - unused */
  uint64_t saved_str_size;
  /** Sizeof of whole PATTY file */
  uint64_t size;
  /** Total size of lookup base string */
  uint64_t lookup_length;
  /** Total size of user-data lookup base */
  uint64_t data_lookup_length;
  /** Root node placement in the PATTY file */
  uint64_t root_offset;
  /** Offset of data section in the PATTY file */
  uint64_t data_lookup_offset;
} patricia_header;


typedef struct patricia_c {
  /** Patricia root node */
  patricia_node_t * root;
  /** Lookup base string for storing literals */
  char * lookup_str;
  /** Lookup base string size */
  uint64_t lookup_sz;

  /** Data lookup */
  void * data_lookup;
  /** Data lookup size */
  size_t data_lookup_sz;

  /** File header when PATRICIA trie is loaded from file */
  patricia_header * file_header;
  /** Mmaped pointer representing PATRICIA trie */
  stream_file_c* mmap_binary;

  /** Base string stream */
  stream_c* csv;


  /**
   * Inserts a word into the trie. Do not use insert method on mmaped trie
   * - insert() is equal to NULL then.
   *
   * @param self An instance of a PATRICIA trie.
   * @param str A string to insert.
   * @param str_start Starting position within the string.
   * @param str_len Length of the string in bytes (without \0).
   *
   * @return A terminal node.
   */
  patricia_node_t* (*insert)(struct patricia_c* self, char* str, uint64_t str_start, uint32_t str_len);

  /**
   * Searches for a word or its prefix in the trie.
   *
   * @param self An instance of a PATRICIA trie.
   * @param str A string to search for.
   * @param str_len Length of the string in bytes (without \0).
   *
   * @return A value in range 0..str_len, where 0 means no match was found and
   * str_len means an exact match was found.
   */
  uint32_t (*search)(struct patricia_c* self, char* str, uint32_t str_len);

  /**
   * Searches for a word or its prefix in the trie.
   *
   * @param self An instance of a PATRICIA trie.
   * @param str A string to search for.
   * @param str_len Length of the string in bytes (without \0).
   * @param node Last node visited during computation, if (*node)->is_terminal
   * then complete match reached.
   *
   * @return A value in range 0..str_len, where 0 means no match was found and
   * str_len means an exact match was found.
   */
  uint32_t (*search_ext)(struct patricia_c* self, char* str, uint32_t str_len, patricia_node_t** node);

  /**
   * Prints the trie into the console.
   *
   * @param self An instance of a PATRICIA trie.
   */
  void (*print)(struct patricia_c* self);

  /**
   * Frees memory used by a PATRICIA trie.
   *
   * @param self An instance of a PATRICIA trie.
   */
  void (*destroy)(struct patricia_c* self);

  /**
   * Saves PATRICIA trie to a file in PATTY format (.patty extension is
   * recommended).
   *
   * @param self An instance of a PATRICIA trie.
   * @param path Path to the file.
   *
   * @return True if save process passed, false otherwise
   */
  bool (*save)(struct patricia_c* self, const char * path);

  /**
   * Traverses whole PATRICIA trie and applies node/edge functions in preorder
   * or postorder order.
   *
   * @param self An instance of a PATRICIA trie
   * @param flags Flags variable filled with TRAVERSE_NODE_PREODER,
   * TRAVERSE_EDGE_PREODER, TRAVERSE_NODE_POSTORDER or TRAVERSE_EDGE_POSTORDER
   * constants.
   * @param n_traversal_fn Function applied on a patricia_node_t * n, takes flag
   * in which order is called and takes context for user data - may be NULL.
   * @param e_traversal_fn Function applied on a patricia_edge_t * e, takes flag
   * in which order is called and takes context for user data - may be NULL.
   * @param context User defined data passed to traversal functions.
   *
   * @return void
  */
  void (*traverse)(struct patricia_c * self, unsigned flags,
    void (*n_traversal_fn)(patricia_node_t * n, unsigned flags, void * context),
    void (*e_traversal_fn)(patricia_edge_t e, unsigned flags, void * context),
    void * context);

  void * (*get)(struct patricia_c * self, const char * str);
  void * (*set)(struct patricia_c * self, const char * str, uint64_t str_start, uint32_t str_len, void * data, size_t data_size);
  void * (*set_data)(struct patricia_c * self, patricia_node_t * node, void * data, size_t data_sz);
} patricia_c;

/**
 * Creates a new empty instance of a PATRICIA trie.
 *
 * @param stream An opened stream or NULL. WARNING: If a stream is passed, then
 * all inserted strings must be from the stream!
 *
 * @return The created trie.
 */
patricia_c* patricia_c_create(stream_c * stream);

/**
 * Creates a new instance of a PATRICIA trie and fills it with words from
 * given CSV file.
 *
 * @param fullpath Path to the CSV file.
 *
 * @return The created trie.
 */
patricia_c* patricia_c_create_from_stream(stream_c * stream);

/**
 * Creates a new instance of a PATRICIA from a binary file in PATTY format.
 *
 * @param fullpath Path to the PATTY file.
 *
 * @return The newly created instance of PATRICIA trie.
 */
patricia_c* patricia_c_from_file(const char* fullpath);

/**
 * Initializes in instance of a PATRICIA trie.
 *
 * @param stream Initializes PATRICIA trie with opened stream or NULL.
 */
void patricia_c_init(patricia_c* self, stream_c * stream);

#endif // PATRICIA_H

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

#ifndef FINITE_AUTOMATON_H
#define FINITE_AUTOMATON_H

#include <nativeextractor/common.h>

typedef size_t fa_id_t;

typedef struct fa_node_t {
  fa_id_t id;
  bool is_starting;
  bool is_final;
  struct fa_edge_t *edges;
} fa_node_t;

typedef struct fa_edge_t {
  fa_id_t id;
  const char *symbol;
  struct fa_node_t *from;
  struct fa_node_t *to;
  struct fa_edge_t *next;
} fa_edge_t;

typedef struct fa_t {
  fa_id_t next_node_id;
  fa_id_t next_edge_id;
  fa_node_t **nodes;
  fa_edge_t **edges;
  size_t node_array_size;
  size_t edge_array_size;
} fa_t;

/** Creates a new finite automaton. */
fa_t *fa_create();

/** Destroys a finite automaton */
void fa_destroy(fa_t *fa);

/** Adds a new node to a finite automaton. */
fa_id_t fa_add_node(fa_t *fa);

/** Returns an internal representation of a node with given id. */
static inline fa_node_t *fa_get_node(fa_t *fa, fa_id_t id) {
  return fa->nodes[id];
}

/** Adds a new edge to a finite automaton. */
fa_id_t fa_add_edge(fa_t *fa, fa_id_t from, const char *symbol, fa_id_t to);

/** Returns an internal representation of an edge with given id. */
static inline fa_edge_t *fa_get_edge(fa_t *fa, fa_id_t id) {
  return fa->edges[id];
}

/** Creates a new DFA from a NFA. */
fa_t *fa_create_powerset(fa_t *fa);

/** Prints a finite automaton to the console. */
void fa_print(fa_t *fa);

#endif  // FINITE_AUTOMATON_H

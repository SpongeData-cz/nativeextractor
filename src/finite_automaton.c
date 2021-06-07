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

#include <nativeextractor/finite_automaton.h>

#include <glib-2.0/glib.h>
#include <string.h>

#define BUCKET_SIZE 10

/** Mapping of multiple nodes from NFA to one node of DFA. */
typedef struct fa_mapping_t {
  size_t count;
  fa_id_t *from;
  fa_id_t to;
  struct fa_mapping_t *prev;
  struct fa_mapping_t *next;
} fa_mapping_t;

static void fa_print_impl(fa_t *fa, fa_mapping_t *mapping);

fa_t *fa_create() {
  fa_t *fa = ALLOC(fa_t);
  fa->next_node_id = 0;
  fa->next_edge_id = 0;
  fa->nodes = (fa_node_t **)calloc(BUCKET_SIZE, sizeof(fa_node_t *));
  fa->edges = (fa_edge_t **)calloc(BUCKET_SIZE, sizeof(fa_edge_t *));
  fa->node_array_size = BUCKET_SIZE;
  fa->edge_array_size = BUCKET_SIZE;
  return fa;
}

static void fa_destroy_node(fa_node_t *node) {
  free(node);
}

static void fa_destroy_edge(fa_edge_t *edge) {
  free(edge);
}

void fa_destroy(fa_t *fa) {
  for (size_t i = 0; i < fa->next_node_id; ++i) {
    fa_destroy_node(fa->nodes[i]);
  }
  free(fa->nodes);

  for (size_t i = 0; i < fa->next_edge_id; ++i) {
    fa_destroy_edge(fa->edges[i]);
  }
  free(fa->edges);

  free(fa);
}

static fa_node_t *fa_node_create(fa_id_t id) {
  fa_node_t *node = ALLOC(fa_node_t);
  node->id = id;
  node->is_starting = false;
  node->is_final = false;
  node->edges = NULL;
  return node;
}

static void fa_node_add_edge(fa_node_t *node, fa_edge_t *edge) {
  fa_edge_t *next_orig = node->edges;
  node->edges = edge;
  edge->next = next_orig;
}

static fa_edge_t *fa_edge_create(fa_id_t id, fa_node_t *from, const char *symbol, fa_node_t *to) {
  fa_edge_t *edge = ALLOC(fa_edge_t);
  edge->id = id;
  edge->symbol = symbol;
  edge->from = from;
  edge->to = to;
  edge->next = NULL;
  return edge;
}

static bool fa_edge_cmp(fa_edge_t *edge, const char *symbol) {
  if (symbol) {
    if (edge->symbol) {
      return (strcmp(edge->symbol, symbol) == 0);
    }
  } else if (!edge->symbol) {
    return true;
  }
  return false;
}

static fa_edge_t *fa_node_find_edge(fa_node_t *node, const char *symbol) {
  fa_edge_t *edge = node->edges;
  while (edge) {
    if (fa_edge_cmp(edge, symbol)) {
      return edge;
    }
    edge = edge->next;
  }
  return NULL;
}

fa_id_t fa_add_node(fa_t *fa) {
  fa_id_t id = fa->next_node_id++;

  if (id >= fa->node_array_size) {
    fa->node_array_size += BUCKET_SIZE;
    fa->nodes = (fa_node_t **)realloc(fa->nodes, sizeof(fa_node_t *) * fa->node_array_size);
  }

  fa->nodes[id] = fa_node_create(id);
  return id;
}

fa_id_t fa_add_edge(fa_t *fa, fa_id_t from, const char *symbol, fa_id_t to) {
  fa_id_t id = fa->next_edge_id++;

  if (id >= fa->edge_array_size) {
    fa->edge_array_size += BUCKET_SIZE;
    fa->edges = (fa_edge_t **)realloc(fa->edges, sizeof(fa_edge_t *) * fa->edge_array_size);
  }

  fa_node_t *node_from = fa->nodes[from];
  fa_node_t *node_to = fa->nodes[to];

  fa_edge_t *edge = fa_edge_create(id, node_from, symbol, node_to);
  fa->edges[id] = edge;

  fa_node_add_edge(node_from, edge);

  return id;
}

static fa_mapping_t *fa_mapping_create(size_t count, fa_id_t *from, fa_id_t to) {
  fa_mapping_t *mapping = ALLOC(fa_mapping_t);
  mapping->count = count;
  if (count > 0) {
    mapping->from = (fa_id_t *)calloc(count, sizeof(fa_id_t));
    memcpy(mapping->from, from, sizeof(fa_id_t) * count);
  } else {
    mapping->from = NULL;
  }
  mapping->to = to;
  mapping->prev = NULL;
  mapping->next = NULL;
  return mapping;
}

static bool fa_mapping_extend(fa_mapping_t *mapping, size_t from_count, fa_id_t *from) {
  bool extended = false;
  for (size_t j = 0; j < from_count; ++j) {
    size_t _from = from[j];
    bool do_extend = true;
    for (size_t i = 0; i < mapping->count; ++i) {
      if (mapping->from[i] == _from) {
        do_extend = false;
        break;
      }
    }
    if (do_extend) {
      mapping->from = (fa_id_t *)realloc(mapping->from, sizeof(fa_id_t) * (mapping->count + 1));
      mapping->from[mapping->count] = _from;
      ++mapping->count;
    }
  }
  return extended;
}

static fa_mapping_t *fa_mapping_find_from(fa_mapping_t *mapping, size_t count, fa_id_t *from) {
  while (mapping) {
    if (mapping->count == count) {
      size_t found = 0;
      for (size_t i = 0; i < count; ++i) {
        for (size_t j = 0; j < count; ++j) {
          if (from[i] == mapping->from[j]) {
            ++found;
            break;
          }
        }
      }
      if (found == count) {
        return mapping;
      }
    }
    mapping = mapping->next;
  }
  return NULL;
}

static fa_mapping_t *fa_mapping_find_to(fa_mapping_t *mapping, fa_id_t to) {
  while (mapping) {
    if (mapping->to == to) {
      return mapping;
    }
    mapping = mapping->next;
  }
  return NULL;
}

static void fa_mapping_add(fa_mapping_t *mapping, fa_mapping_t *next) {
  fa_mapping_t *next_orig = mapping->next;
  mapping->next = next;
  next->prev = mapping;
  next->next = next_orig;
  if (next_orig) {
    next_orig->prev = next;
  }
}

static void fa_mapping_destroy(fa_mapping_t *mapping) {
  while (mapping) {
    fa_mapping_t *next = mapping->next;
    if (mapping->from) {
      free(mapping->from);
    }
    free(mapping);
    mapping = next;
  }
}

/** Creates a list of unique edge symbols from node mapping. Don't forget
 * to destroy the list when it's no longer necessary! */
static GSList *fa_mapping_get_edge_symbols(fa_mapping_t *mapping, fa_t *fa) {
  GSList *symbols = NULL;
  for (size_t i = 0; i < mapping->count; ++i) {
    fa_node_t *node_fa = fa_get_node(fa, mapping->from[i]);
    fa_edge_t *edge = node_fa->edges;
    while (edge) {
      if (edge->symbol) {
        if (!g_slist_find(symbols, (gpointer)edge->symbol)) {
          symbols = g_slist_append(symbols, (gpointer)edge->symbol);
        }
      }
      edge = edge->next;
    }
  }
  return symbols;
}

static void fa_gather_transition_nodes(fa_node_t *node, const char *symbol, size_t *temp_count, fa_id_t *temp) {
  fa_edge_t *edge = node->edges;
  while (edge) {
    if (fa_edge_cmp(edge, symbol)) {
      fa_node_t *node_to = edge->to;
      bool added = false;
      for (size_t i = 0; i < *temp_count; ++i) {
        if (temp[i] == node_to->id) {
          // Node already in temp
          added = true;
          break;
        }
      }
      if (!added) {
        temp[(*temp_count)++] = node_to->id;
      }
      fa_gather_transition_nodes(node_to, NULL, temp_count, temp);
    }
    edge = edge->next;
  }
}

/**
 * @param fa NFA
 * @param powerset DFA
 * @param mapping From NFA to DFA
 * @param node DFA node
 * @return True if something was modified
 */
static bool fa_create_powerset_impl(fa_t *fa, fa_t *powerset, fa_mapping_t *mapping, fa_id_t node) {
  fa_node_t *node_impl = fa_get_node(powerset, node);
  fa_mapping_t *node_mapping = fa_mapping_find_to(mapping, node);
  GSList *unique_symbols = fa_mapping_get_edge_symbols(node_mapping, fa);
  GSList *current_symbol = unique_symbols;

  for (size_t j = 0; j < node_mapping->count; ++j) {
    fa_node_t *node_fa = fa_get_node(fa, node_mapping->from[j]);
    if (node_fa->is_final) {
      node_impl->is_final = true;
    }
  }

  for (size_t i = 0; i < g_slist_length(unique_symbols); ++i) {
    const char *symbol = (const char *)current_symbol->data;
    current_symbol = current_symbol->next;

    if (fa_node_find_edge(node_impl, symbol)) {
      continue;
    }

    fa_id_t temp[fa->next_node_id];
    fa_id_t temp_count = 0;

    for (size_t j = 0; j < node_mapping->count; ++j) {
      fa_node_t *node_fa = fa_get_node(fa, node_mapping->from[j]);
      fa_gather_transition_nodes(node_fa, symbol, &temp_count, temp);
    }

    if (temp_count > 0) {
      fa_mapping_t *m = fa_mapping_find_from(mapping, temp_count, temp);
      fa_id_t node_to;

      if (m) {
        node_to = m->to;
        fa_add_edge(powerset, node, symbol, node_to);
      } else {
        node_to = fa_add_node(powerset);
        fa_add_edge(powerset, node, symbol, node_to);

        m = fa_mapping_create(temp_count, temp, node_to);
        fa_mapping_add(mapping, m);
      }

      fa_create_powerset_impl(fa, powerset, mapping, node_to);
    }
  }

  g_slist_free(unique_symbols);
}

fa_t *fa_create_powerset(fa_t *fa) {
  fa_id_t temp[fa->next_node_id];
  fa_id_t temp_count = 0;

  fa_t *powerset = fa_create();
  fa_node_t *start = fa_get_node(powerset, fa_add_node(powerset));
  start->is_starting = true;

  for (size_t i = 0; i < fa->next_node_id; ++i) {
    fa_node_t *node = fa_get_node(fa, i);
    if (node->is_starting) {
      temp[temp_count++] = node->id;
    }
  }

  for (size_t i = 0; i < fa->next_node_id; ++i) {
    fa_node_t *node = fa_get_node(fa, i);
    if (node->is_starting) {
      fa_gather_transition_nodes(node, NULL, &temp_count, temp);
    }
  }

  fa_mapping_t *mapping = fa_mapping_create(temp_count, temp, start->id);

  fa_create_powerset_impl(fa, powerset, mapping, start->id);
// #ifdef DEBUG
//   fa_print_impl(powerset, mapping);
// #endif
  fa_mapping_destroy(mapping);

  return powerset;
}

static void fa_print_node(fa_node_t *node, fa_mapping_t *mapping, bool show_starting) {
  fa_mapping_t *node_mapping =
      mapping ? fa_mapping_find_to(mapping, node->id) : NULL;

  if (show_starting && node->is_starting) {
    printf("-->");
  }

  putchar('(');
  if (node->is_final) {
    putchar('(');
  }

  if (node_mapping) {
    putchar('{');
    for (size_t i = 0; i < node_mapping->count; ++i) {
      printf("q%lu,", node_mapping->from[i]);
    }
    putchar('}');
  } else {
    printf("q%lu", node->id);
  }

  if (node->is_final) {
    putchar(')');
  }
  putchar(')');
}

static void fa_print_edge(fa_edge_t *edge) {
  printf("--%s-->", edge->symbol ? edge->symbol : "ε");
}

static void fa_print_impl(fa_t *fa, fa_mapping_t *mapping) {
  for (size_t i = 0; i < fa->next_node_id; ++i) {
    fa_node_t *node = fa->nodes[i];
    fa_edge_t *edge = node->edges;

    if (!edge) {
      fa_print_node(node, mapping, true);
      printf("\n");
      continue;
    }

    while (edge) {
      fa_print_node(node, mapping, true);
      fa_print_edge(edge);
      fa_print_node(edge->to, mapping, false);
      printf("\n");
      edge = edge->next;
    }
  }
}

void fa_print(fa_t *fa) {
  fa_print_impl(fa, NULL);
}

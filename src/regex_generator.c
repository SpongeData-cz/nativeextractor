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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib-2.0/glib.h>
#include <nativeextractor/regex_generator.h>
#include <nativeextractor/extractor.h>
#include <nativeextractor/unicode.h>
#include <nativeextractor/terminal.h>

enum atom_type_e {
  ROOT,
  // {(
  L_PARENTHESIS,
  // })
  R_PARENTHESIS,
  // a|b
  ALTERNATION,
  // ^
  NEGATION,
  // +,*,?
  CLOSURE,
  CHAR,
} atom_type_e;

const char *lex_lookup_human[] = {
  "ROOT",
  "L_PARENTHESIS",
  "R_PARENTHESIS",
  "ALTERNATION",
  "NEGATION",
  "CLOSURE",
  "CHAR"
};

typedef struct lex_atom_t {
  enum atom_type_e type;
  void *data;
  unsigned data_len;
} lex_atom_t;

const char *lex_lookup[] = {
  NULL,
  "({",
  ")}",
  "|",
  "^",
  "+*?",
  "\\",
  NULL
};

static size_t parse_character_group(char *str) {
  const char *str_start = str;

  while (true) {
    if (*str == '\0') {
      // Closing bracket not found
      PRINT_ERROR("Expected ]!");
      return 0;
    }

    if (cmp_unicode(str, "\\")) {
      // Skip over escaped characters
      ++str;
    } else if (cmp_unicode(str, "]")) {
      if (str - str_start == 1) {
        // Empty character group
        PRINT_ERROR("Invalid character group []!");
        return 0;
      }
      if (str - str_start == 2
        && *(char *)(str - 1) == '^') {
        // Negation of empty character group
        PRINT_ERROR("Invalid character group [^]!");
        return 0;
      }
      break;
    }

    str += unicode_getbytesize(str);
  }

  return str - str_start;
}

static lex_atom_t *lex_atom_create(char *str) {
  lex_atom_t *atom = ALLOC(lex_atom_t);

  // Normal chars
  atom->type = CHAR;
  atom->data = str;
  atom->data_len = unicode_getbytesize(str);

  if (cmp_unicode(str, "[")) {
    // Character groups
    size_t len = parse_character_group(str);
    if (len == 0) {
      return NULL;
    }
    str += len;
    atom->data_len += len;
  } else if (cmp_unicode(str, "]")) {
    return NULL;
  } else if (cmp_unicode(str, "\\")) {
    // Escaped chars
    ++atom->data_len;
  } else {
    // Special chars
    for (unsigned ai = L_PARENTHESIS; ai < CHAR; ++ai) {
      char *ptr = (char *)lex_lookup[ai];
      while (*ptr) {
        if (*ptr == *str) {
          atom->type = ai;
          return atom;
        }
        ++ptr;
      }
    }
  }

  return atom;
}

static lex_atom_t *lex_atom_create_root() {
  lex_atom_t *atom = ALLOC(lex_atom_t);
  atom->type = ROOT;
  atom->data = NULL;
  atom->data_len = 0;
  return atom;
}

static void lex_atom_destroy(lex_atom_t *lex_atom) {
  free(lex_atom);
}

static const char get_closing_parens(const char left) {
  const char *lb = lex_lookup[L_PARENTHESIS];
  int offset = 0;

  while (*lb) {
    if (*lb == left) {
      return lex_lookup[R_PARENTHESIS][offset];
    }
    ++offset;
    ++lb;
  }

  PRINT_ERROR("Invalid parenthesis %c!", left);
  assert(false);
}

static const char *lex_tree_create_impl(GNode *parent, const char *str) {
  lex_atom_t *prev_atom = NULL;
  lex_atom_t *parent_atom = (lex_atom_t *)parent->data;

  while (*str) {
    lex_atom_t *atom = lex_atom_create((char *)str);
    if (!atom) {
      return NULL;
    }

    GNode *node = NULL;

    node = g_node_new(atom);
    node = g_node_insert(parent, -1, node);
    prev_atom = atom;

    if (atom->type == L_PARENTHESIS) {
      str = lex_tree_create_impl(node, str + atom->data_len);
    } else if (atom->type == R_PARENTHESIS) {
      char *parent_data = (char *)((lex_atom_t *)parent->data)->data;
      if (get_closing_parens(*parent_data) != *str) {
        PRINT_ERROR("Mismatched parentheses - got %c, expected %c!",
          *str, get_closing_parens(*parent_data));
        return NULL;
      }
      return str;
    }

    if (!str) {
      return NULL;
    }

    str += atom->data_len;
  }

  return str;
}

static GNode *lex_tree_create(const char *str) {
  lex_atom_t *root_data = lex_atom_create_root();
  GNode *root = g_node_new(root_data);

  const char *ret = lex_tree_create_impl(root, str);
  if (!ret) {
    g_node_destroy(root);
    return NULL;
  }

  return root;
}

static void lex_tree_destroy(GNode *tree) {
  GNode *child = tree->children;
  while (child) {
    GNode *next = child->next;
    lex_tree_destroy(child);
    child = next;
  }
  lex_atom_destroy(tree->data);
  g_node_unlink(tree);
  g_node_destroy(tree);
}

static void lex_tree_print(GNode *tree, int level) {
  while (tree) {
    lex_atom_t *atom = (lex_atom_t *)tree->data;
    for (int l = 0; l < level; ++l) {
      printf("\t");
    }
    char *to_print = g_strndup((char *)atom->data, atom->data_len);
    printf("(%s) %s\n", lex_lookup_human[atom->type], to_print);
    free(to_print);
    lex_tree_print(tree->children, level + 1);
    tree = tree->next;
  }
}

enum op_type_e {
  OP_CONCAT,
  // [...]
  OP_SET,
  // a|b
  OP_ALTERNATION,
  OP_NEGATION,
  // ^
  OP_LINEBEGIN,
  // {#}, {#,}, {#,#}
  OP_RANGE,
  // +,*,?
  OP_CLOSURE,
  OP_IDENTITY,
  OP_UNKNOWN
} op_type_e;

const char *human_op_type[] = {
  "OP_CONCAT",
  "OP_SET",
  "OP_ALTERNATION",
  "OP_NEGATION",
  "OP_LINEBEGIN",
  "OP_RANGE",
  "OP_CLOSURE",
  "OP_IDENTITY",
  "OP_UNKNOWN"
};

typedef struct re_op_t {
  enum op_type_e type;
  char *symbol;
  void *data;
} re_op_t;

static re_op_t *re_op_create(enum op_type_e type, const char *symbol, void *data) {
  re_op_t *op = ALLOC(re_op_t);
  op->type = type;
  op->symbol = g_strdup(symbol);
  op->data = data;
  return op;
}

static void re_op_destroy(re_op_t *op) {
  if (op->symbol) {
    free(op->symbol);
  }
  if (op->data) {
    free(op->data);
  }
  free(op);
}

static void op_tree_destroy(GNode *tree) {
  GNode *child = tree->children;
  while (child) {
    GNode *next = child->next;
    op_tree_destroy(child);
    child = next;
  }
  re_op_destroy(tree->data);
  g_node_unlink(tree);
  g_node_destroy(tree);
}

static GNode *op_root(GNode *lex_node, GNode **current_op_node) {
  lex_atom_t *atom = (lex_atom_t *)lex_node->data;
  char *symbol = (char *)atom->data;

  re_op_t *op = re_op_create(OP_CONCAT, symbol, NULL);
  GNode *out = g_node_new(op);
  *current_op_node = out;

  return lex_node->children;
}

static void op_print_tree(GNode *tree, int level) {
  while (tree) {
    re_op_t *op = (re_op_t *)tree->data;
    for (int l = 0; l < level; ++l) {
      printf("\t");
    }
    printf("(%s) %s\n", human_op_type[op->type], op->symbol);
    op_print_tree(tree->children, level + 1);
    tree = tree->next;
  }
}

/** Use values >= 0 or -1 for no upper bound. */
typedef struct range_t {
  int l;
  int h;
} range_t;

static bool op_parse_int(GNode **node, int *i) {
  bool success = false;
  *i = 0;

  while (*node) {
    re_op_t *node_data = (re_op_t *)(*node)->data;
    char* symbol = node_data->symbol;

    if (unicode_isdigit(symbol)) {
      success = true;
      *i = ((*i) * 10) + atoi(symbol);
    } else {
      break;
    }

    *node = (*node)->next;
  }

  return success;
}

static range_t *op_get_range(GNode* range_child) {
  GNode* child = range_child;

  int l = 0;
  int h = 0;

  // Parse lower bound
  if (!op_parse_int(&child, &l)) {
    PRINT_ERROR("{} range requires at least one argument");
    return NULL;
  }

  // Comma (optional)
  if (child && *((re_op_t *)child->data)->symbol == ',') {
    child = child->next;

    // Parse upper bound (optional)
    if (!op_parse_int(&child, &h)) {
      h = -1;
    }

  } else {
    h = l;
  }

  if (h != -1 && l > h) {
    PRINT_ERROR("{} invalid range {%d,%d}", l, h);
    return NULL;
  }

  if (child) {
    PRINT_ERROR("{} unexpected token %s", ((re_op_t *)child->data)->symbol);
    return NULL;
  }

  range_t *range = ALLOC(range_t);
  range->l = l;
  range->h = h;
  // printf("range_t { l: %d, h: %d }\n", l, h);
  return range;
}

static GNode *lex_to_partial_op(GNode *root) {
  if (!root) {
    return NULL;
  }

  lex_atom_t *atom = (lex_atom_t *)root->data;

  char *_symbol = g_strndup((char *)atom->data, atom->data_len);
  re_op_t *op = re_op_create(OP_UNKNOWN, _symbol, NULL);
  free(_symbol);

  GNode *last_child = g_node_last_child(root);

  switch (atom->type) {
    case CHAR:
      op->type = OP_IDENTITY;
      break;

    case ROOT:
      op->type = OP_CONCAT;
      break;

    case L_PARENTHESIS:
      if (*op->symbol == '(') {
        op->type = OP_CONCAT;
      } else if (*op->symbol == '[') {
        op->type = OP_SET;
      } else {
        op->type = OP_RANGE;
      }
      break;

    case R_PARENTHESIS:
      re_op_destroy(op);
      return NULL;
      break;

    case ALTERNATION:
      op->type = OP_ALTERNATION;
      break;

    case NEGATION:
      op->type = OP_IDENTITY;

      if (root->parent) {
        lex_atom_t *lc_atom = (lex_atom_t *)(root->parent->data);
        char *data = (char *)lc_atom->data;
        if (data && *data == '[') {
          op->type = OP_NEGATION;
        }
      }
      break;

    case CLOSURE:
      op->type = OP_CLOSURE;
      break;

    default:
      PRINT_ERROR("Unknown op %d", atom->type);
      break;
  }

  GNode *out = g_node_new(op);
  GNode *child = root->children;

  while (child) {
    GNode *dup_child = lex_to_partial_op(child);

    if (dup_child) {
      re_op_t *c_op = (re_op_t *)dup_child->data;

      if (c_op->type == OP_RANGE) {
        last_child = g_node_last_child(out);

        if (!last_child) {
          PRINT_ERROR("{} range must be preceeded by an atom.");
          return NULL;
        }

        GNode *range_child = dup_child->children;

        if (!range_child) {
          PRINT_ERROR("{} range must have arguments.");
          return NULL;
        }

        re_op_t *c_op = (re_op_t *)dup_child->data;

        range_t *range = op_get_range(range_child);
        if (!range) {
          return NULL;
        }
        c_op->data = range;

        GNode* child = range_child;
        while (child) {
          GNode* next = child->next;
          if (child->data) {
            re_op_destroy(child->data);
          }
          g_node_unlink(child);
          g_node_destroy(child);
          child = next;
        }

        g_node_unlink(last_child);
        g_node_insert(dup_child, -1, last_child);
      }

      g_node_insert(out, -1, dup_child);
    }

    child = child->next;
  }

  return out;
}

/** Removes unnecessary concats that have only one child node. */
static GNode *op_tree_collapse_concats(GNode *node) {
  if (!node) {
    return node;
  }

  GNode *child;
  GNode *node_orig = node;
  enum op_type_e type = ((re_op_t *)node->data)->type;

  child = node_orig->children;
  while (child) {
    GNode *next = child->next;
    op_tree_collapse_concats(child);
    child = next;
  }

  if (type == OP_CONCAT && node->children->next == NULL) {
    child = node->children;
    g_node_unlink(child);
    if (!G_NODE_IS_ROOT(node)) {
      g_node_insert_after(node->parent, node, child);
    }
    g_node_unlink(node);
    if (node->data) {
      re_op_destroy(node->data);
    }
    g_node_destroy(node);
    node_orig = child;
  }

  return node_orig;
}

/** Creates a new concat node from all left (-1) or right (+1) siblings of a node. */
static GNode *concat_context(GNode *node, int dir) {
  assert(dir != 0);

  re_op_t *concat = re_op_create(OP_CONCAT, "(", NULL);
  GNode *context = g_node_new(concat);

  GNode *current = (dir < 0) ? node->prev : node->next;
  while (current) {
    GNode *next = (dir < 0) ? current->prev : current->next;
    g_node_unlink(current);
    g_node_insert(context, (dir < 0) ? 0 : -1, current);
    current = next;
  }

  return context;
}

static GNode *transform_alternation(GNode *re_op_partial_tree) {
  GNode *child = re_op_partial_tree->children;

  while (child) {
    GNode *next_orig = child->next;

    if (!transform_alternation(child)) {
      return NULL;
    }

    re_op_t *op = (re_op_t *)child->data;

    if (op->type == OP_ALTERNATION) {
      GNode *prev_child = child->prev;
      if (!prev_child) {
        PRINT_ERROR("Alternation | missing left context");
        return NULL;
      }

      GNode *next_child = child->next;
      if (!next_child) {
        PRINT_ERROR("Alternation | missing right context");
        return NULL;
      }

      g_node_insert(child, -1, concat_context(child, -1));
      g_node_insert(child, -1, concat_context(child, +1));
    }

    child = next_orig;
  }

  return re_op_partial_tree;
}

static GNode *transform_closures(GNode *root) {
  re_op_t *op = (re_op_t *)root->data;

  if (op->type == OP_CLOSURE) {
    GNode *prev = root->prev;
    if (!prev) {
      printf("Closure missing left operand\n");
      return NULL;
    }

    g_node_unlink(prev);
    g_node_insert(root, -1, prev);
    return root;
  }

  GNode *child = root->children;
  while (child) {
    if (!transform_closures(child)) {
      return NULL;
    }
    child = child->next;
  }

  return root;
}

static GNode *lex_to_op(GNode *root) {
  GNode *form = lex_to_partial_op(root);

  if (!transform_closures(form)) {
    PRINT_ERROR("Compiling regex failed.");
    op_tree_destroy(form);
    return NULL;
  }

  if (!transform_alternation(form)) {
    PRINT_ERROR("Compiling regex failed.");
    op_tree_destroy(form);
    return NULL;
  }

  return form;
}

static gchar *join_str(GList *lst_str, const char *delimiter) {
  if (!lst_str) {
    return g_strdup("");
  }

  gchar *accum = g_strdup((const gchar *)lst_str->data);

  lst_str = lst_str->next;

  while (lst_str) {
    gchar *tmp = accum;
    accum = g_strdup_printf("%s%s%s", accum, delimiter, (const gchar *)lst_str->data);
    free(tmp);
    lst_str = lst_str->next;
  }

  return accum;
}

/** A pair of finite automata nodes. Used to return starting and final nodes
 * from functions. */
typedef struct fa_pair_t {
  fa_id_t a;
  fa_id_t b;
} fa_pair_t;

static fa_pair_t re_op_to_nfa(GNode *tree, fa_t *nfa);

// See: https://en.wikipedia.org/wiki/Thompson%27s_construction
// See: http://alexandria.tue.nl/extra1/wskrap/publichtml/9313452.pdf

static fa_pair_t identity_to_nfa(GNode *tree, fa_t *nfa) {
  re_op_t *op = (re_op_t *)tree->data;
  fa_id_t from = fa_add_node(nfa);
  fa_id_t to = fa_add_node(nfa);
  fa_add_edge(nfa, from, op->symbol, to);
  return (fa_pair_t){from, to};
}

static fa_pair_t concat_to_nfa(GNode *tree, fa_t *nfa) {
  bool first = true;
  fa_id_t from;
  fa_id_t node;

  GNode *child = tree->children;
  while (child) {
    fa_pair_t pair = re_op_to_nfa(child, nfa);
    if (first) {
      from = pair.a;
      first = false;
    } else {
      fa_add_edge(nfa, node, NULL, pair.a);
    }
    node = pair.b;
    child = child->next;
  }

  return (fa_pair_t){from, node};
}

static fa_pair_t alternation_to_nfa(GNode *tree, fa_t *nfa) {
  fa_id_t from = fa_add_node(nfa);
  fa_id_t to = fa_add_node(nfa);

  GNode *child = tree->children;
  while (child) {
    fa_pair_t pair = re_op_to_nfa(child, nfa);
    fa_add_edge(nfa, from, NULL, pair.a);
    fa_add_edge(nfa, pair.b, NULL, to);
    child = child->next;
  }

  return (fa_pair_t){from, to};
}

static fa_pair_t closure_to_nfa(GNode *tree, fa_t *nfa) {
  re_op_t *op = (re_op_t *)tree->data;
  fa_id_t from = fa_add_node(nfa);
  fa_id_t to = fa_add_node(nfa);

  // Can skip in * and ?
  if (*op->symbol != '+') {
    fa_add_edge(nfa, from, NULL, to);
  }

  fa_pair_t pair = re_op_to_nfa(tree->children, nfa);
  fa_add_edge(nfa, from,  NULL, pair.a);
  fa_add_edge(nfa, pair.b,  NULL, to);

  // Can loop in + and *
  if (*op->symbol != '?') {
    fa_add_edge(nfa, pair.b,  NULL, pair.a);
  }

  return (fa_pair_t){from, to};
}

static GNode *tree_clone_create(GNode *tree) {
  GNode *clone = g_node_new(tree->data);
  GNode *child = tree->children;
  while (child) {
    g_node_append(clone, tree_clone_create(child));
    child = child->next;
  }
  return clone;
}

static void tree_clone_destroy(GNode *tree) {
  GNode *child = tree->children;
  while (child) {
    g_node_destroy(child);
    child = child->next;
  }
  g_node_destroy(tree);
}

static fa_pair_t range_to_nfa(GNode *tree, fa_t *nfa) {
  re_op_t *op = (re_op_t *)tree->data;
  range_t *range = (range_t *)op->data;
  fa_id_t from = fa_add_node(nfa);
  fa_id_t to = fa_add_node(nfa);
  fa_id_t node = from;

  // Lower
  for (int i = 0; i < range->l; ++i) {
    fa_pair_t pair = re_op_to_nfa(tree->children, nfa);
    fa_add_edge(nfa, node, NULL, pair.a);
    node = pair.b;
  }

  // Upper
  re_op_t clone_op = (re_op_t){OP_CLOSURE, NULL, NULL};
  GNode *clone_root = g_node_new(&clone_op);
  GNode *clone_children = tree_clone_create(tree->children);
  g_node_append(clone_root, clone_children);
  if (range->h == -1) {
    clone_op.symbol = "*";
    fa_pair_t pair = re_op_to_nfa(clone_root, nfa);
    fa_add_edge(nfa, node, NULL, pair.a);
    node = pair.b;
  } else {
    clone_op.symbol = "?";
    for (int i = range->l; i < range->h; ++i) {
      fa_pair_t pair = re_op_to_nfa(clone_root, nfa);
      fa_add_edge(nfa, node, NULL, pair.a);
      node = pair.b;
    }
  }
  g_node_destroy(clone_root);

  fa_add_edge(nfa, node, NULL, to);
  return (fa_pair_t){from, to};
}

static fa_pair_t re_op_to_nfa(GNode *tree, fa_t *nfa) {
  re_op_t *op = (re_op_t *)tree->data;
  fa_pair_t retval;

  switch (op->type) {
    case OP_IDENTITY:
      return identity_to_nfa(tree, nfa);
      break;

    case OP_CONCAT:
      return concat_to_nfa(tree, nfa);
      break;

    case OP_ALTERNATION:
      return alternation_to_nfa(tree, nfa);
      break;

    case OP_CLOSURE:
      return closure_to_nfa(tree, nfa);
      break;

    case OP_RANGE:
      return range_to_nfa(tree, nfa);
      break;

    default:
      PRINT_ERROR("%s %s not yet implemented!", human_op_type[op->type], op->symbol);
      assert(false);
      break;
  }
}

static fa_t *regex_to_nfa(regex_t *regex) {
  fa_t *nfa = fa_create();
  fa_pair_t pair = re_op_to_nfa(regex->internal_form, nfa);
  fa_get_node(nfa, pair.a)->is_starting = true;
  fa_get_node(nfa, pair.b)->is_final = true;
  return nfa;
}

static void free_string_list(GList **list) {
  GList *current = *list;
  while (current) {
    free(current->data);
    current = current->next;
  }
  g_list_free(g_steal_pointer(list));
}

static gchar *join_str_and_free(GList **code, const char *delimiter) {
  gchar *str = join_str(*code, "");
  free_string_list(code);
  return str;
}

#define TYPE_STRING 0
#define TYPE_FUNCTION 1
#define TYPE_RANGE 2
#define TYPE_LINEBEGIN 3
#define TYPE_LINEEND 4

typedef struct someshit_t {
  int type;
  char *str;
  int from;
  int to;
} someshit_t;

static bool str_to_match_fn(regex_t *re, char **str, bool is_group, someshit_t *out) {
  switch (**str) {
    case '^':
      *str += 1;
      out->type = TYPE_LINEBEGIN;
      out->str = NULL;
      return true;
      break;

    case '$':
      *str += 1;
      out->type = TYPE_LINEEND;
      out->str = NULL;
      return true;
      break;

    case '.':
      if (is_group) {
        // .
        *str += 1;
        out->type = TYPE_STRING;
        out->str = g_strdup(".");
        return true;
      } else {
        // [^\n]
        *str += 1;
        out->type = TYPE_FUNCTION;
        out->str = g_strdup("unicode_not_islinebreak");
        return true;
      }
      break;

    case '\\':
      switch (*(*str + 1)) {
        // Character classes
        case 's':
          // [\r\n\t\f\v]
          *str += 2;
          out->type = TYPE_FUNCTION;
          out->str = g_strdup("unicode_isspace");
          return true;

        case 'S':
          // [^\r\n\t\f\v]
          *str += 2;
          out->type = TYPE_FUNCTION;
          out->str = g_strdup("unicode_not_isspace");
          return true;

        case 'w':
          // [a-zA-Z0-9_]
          *str += 2;
          out->type = TYPE_FUNCTION;
          out->str = g_strdup("unicode_isw");
          return true;

        case 'W':
          // [^a-zA-Z0-9_]
          *str += 2;
          out->type = TYPE_FUNCTION;
          out->str = g_strdup("unicode_not_isw");
          return true;

        case 'd':
          // [0-9]
          *str += 2;
          out->type = TYPE_FUNCTION;
          out->str = g_strdup("unicode_isalpha");
          return true;

        case 'D':
          // [^0-9]
          *str += 2;
          out->type = TYPE_FUNCTION;
          out->str = g_strdup("unicode_not_isalpha");
          return true;

        // Special chars
        case 'n':
          *str += 2;
          out->type = TYPE_STRING;
          out->str = g_strdup("\\n");
          return true;

        case 't':
          *str += 2;
          out->type = TYPE_STRING;
          out->str = g_strdup("\\t");
          return true;

        case 'r':
          *str += 2;
          out->type = TYPE_STRING;
          out->str = g_strdup("\\r");
          return true;

        case 'v':
          *str += 2;
          out->type = TYPE_STRING;
          out->str = g_strdup("\\v");
          return true;

        // Word boundary
        case 'b':
          // TODO: Implement word boundary matching!
          break;

        // Escaped chars
        default: {
          *str += 1;
          size_t bytesize = unicode_getbytesize(*str);
          out->type = TYPE_STRING;
          out->str = g_strndup(*str, bytesize);
          *str += bytesize;
          return true;
        };
      }
      break;

    default: {
      // Character ranges
      if (is_group) {
        if (unicode_isalnum(*str)) {
          char *from = *str;
          size_t from_size = unicode_getbytesize(from);
          int from_int = unicode_to_int(from);

          if (*(*str + from_size) == '-') {
            *str += from_size + 1;

            char *to = *str;
            size_t to_size = unicode_getbytesize(to);
            int to_int = unicode_to_int(to);
            *str += to_size;

            if (!unicode_isalnum(to)
              || from_int > to_int
              || unicode_isalpha(from) != unicode_isalpha(to)
              || unicode_isupper(from) != unicode_isupper(to)) {
              char *range = g_strndup(from, *str - from);
              re->errors = g_list_append(re->errors, g_strdup_printf("Invalid character range %s!", range));
              free(range);
              return false;
            }

            out->type = TYPE_RANGE;
            out->from = from_int;
            out->to = to_int;
            return true;

            // return g_strdup_printf(
            //   "(e->match(e, unicode_isalnum, Right) && unicode_to_int(e->match_last) >= %d && unicode_to_int(e->match_last) <= %d)",
            //   from_int, to_int);
          }
        }
      }

      // Single characters
      size_t bytesize = unicode_getbytesize(*str);
      out->type = TYPE_STRING;
      out->str = g_strndup(*str, bytesize);
      *str += bytesize;
      return true;
    };
  }

  return false;
}

static gchar *edges_to_code(regex_t *re, fa_node_t *node, GList **code_global) {
  GList *code = NULL;
  fa_edge_t *edge = node->edges;

  someshit_t someshit;

  while (edge) {
    code = g_list_append(code, g_strdup("  if (false"));

    char *str = (char *)edge->symbol;
    bool is_group = (*str == '[');
    bool is_negation = false;

    if (is_group) {
      // Handle [] groups
      ++str;

      code = g_list_append(code, g_strdup_printf("\n    || e->match_fn(e, unicode_isgroup_%s_%lu, Right)", re->naming, edge->id));
      *code_global = g_list_append(*code_global, g_strdup_printf("// %s\nstatic bool unicode_isgroup_%s_%lu(char *c) {\n  return (false", edge->symbol, re->naming, edge->id));

      is_negation = (*str == '^');
      if (is_negation) {
        ++str;
        *code_global = g_list_append(*code_global, g_strdup(" || !(false"));
      }

      while (*str != ']') {
        if (!str_to_match_fn(re, &str, is_group, &someshit)) {
          free_string_list(&code);
          return NULL;
        }

        if (someshit.type == TYPE_STRING) {
          *code_global = g_list_append(*code_global, g_strdup_printf("\n    || cmp_unicode(c, \"%s\")", someshit.str));
          free(someshit.str);
        } else if (someshit.type == TYPE_FUNCTION) {
          *code_global = g_list_append(*code_global, g_strdup_printf("\n    || %s(c)", someshit.str));
          free(someshit.str);
        } else if (someshit.type == TYPE_RANGE) {
          *code_global = g_list_append(*code_global, g_strdup_printf("\n    || (unicode_to_int(c) >= %d && unicode_to_int(c) <= %d)", someshit.from, someshit.to));
        } else {
          re->errors = g_list_append(re->errors, g_strdup("An unexpected error has occurred!"));
          free_string_list(&code);
          return NULL;
        }
      }

      if (is_negation) {
        *code_global = g_list_append(*code_global, g_strdup(")"));
      }
      *code_global = g_list_append(*code_global, g_strdup(");\n}\n\n"));

    } else {
      // Single character
      if (!str_to_match_fn(re, &str, is_group, &someshit)) {
        free_string_list(&code);
        return NULL;
      }

      if (someshit.type == TYPE_STRING) {
        code = g_list_append(code, g_strdup_printf("\n    || e->match(e, \"%s\", Right)", someshit.str));
        free(someshit.str);
      } else if (someshit.type == TYPE_FUNCTION) {
        code = g_list_append(code, g_strdup_printf("\n    || e->match_fn(e, %s, Right)", someshit.str));
        free(someshit.str);
      } else if (someshit.type == TYPE_LINEBEGIN) {
        // TODO: Add support for multiline matching.
        code = g_list_append(code, g_strdup("\n    || !e->can_move(e, Left)"));
      } else if (someshit.type == TYPE_LINEEND) {
        // TODO: Add support for multiline matching.
        code = g_list_append(code, g_strdup("\n    || !e->can_move(e, Right)"));
      } else {
        re->errors = g_list_append(re->errors, g_strdup("An unexpected error has occurred!"));
        free_string_list(&code);
        return NULL;
      }
    }

    code = g_list_append(code, g_strdup_printf(
      ") {\n"
      "    return state_%s_%lu(e);\n"
      "  }\n",
      re->naming, edge->to->id
    ));

    edge = edge->next;
  }

  return join_str_and_free(&code, "\n");
}

static gchar *starting_nodes_to_code(regex_t * re, fa_t *dfa) {
  const char *format =
      "  e->reset_pos(e, &mark);\n"
      "  if (state_%s_%lu(e)) {\n"
      "    e->mark_end(e);\n"
      "    return e->make_occurrence(e, 1.0);\n"
      "  }\n";

  GList *code = NULL;

  for (fa_id_t id = 0; id < dfa->next_node_id; ++id) {
    fa_node_t *node = dfa->nodes[id];
    if (!node->is_starting) {
      continue;
    }
    gchar *node_code = g_strdup_printf(format, re->naming, id);
    code = g_list_append(code, node_code);
  }

  return join_str_and_free(&code, "\n");
}

static gchar *regex_dfa_to_code(regex_t *re, fa_t *dfa) {
  const char *format = "";
  GList *code = g_list_append(NULL, g_strdup((gpointer)format));

  // State declarations
  format = "static inline bool state_%s_%lu(miner_c *e);\n";
  for (fa_id_t id = 0; id < dfa->next_node_id; ++id) {
    code = g_list_append(code, g_strdup_printf(format, re->naming, id));
  }
  code = g_list_append(code, g_strdup("\n"));

  // State definitions
  format =
      "static inline bool state_%s_%lu(miner_c *e) {\n"
      "%s"
      "  return %s;\n"
      "}\n\n";
  for (fa_id_t id = 0; id < dfa->next_node_id; ++id) {
    fa_node_t *node = dfa->nodes[id];
    gchar *edges_code = edges_to_code(re, node, &code);

    if (edges_code == NULL) {
      free_string_list(&code);
      return NULL;
    }

    gchar *node_code = g_strdup_printf(
      format, re->naming, id, edges_code, node->is_final ? "true" : "false");
    free(edges_code);
    code = g_list_append(code, node_code);
  }

  // Main
  format =
      "static occurrence_t *match_regex_%s_impl(miner_c *e) {\n"
      "  mark_t mark;\n"
      "  e->mark_pos(e, &mark);\n"
      "  e->mark_start(e);\n\n"
      "%s\n"
      "  e->reset_pos(e, &mark);\n"
      "  return NULL;\n"
      "}\n\n"
      "miner_c* %s() {\n"
      "  return miner_c_create(\"%s\", NULL, match_regex_%s_impl);\n"
      "}\n";
  gchar *starting_nodes = starting_nodes_to_code(re, dfa);

  gchar *re_expr_escaped = g_strescape(re->re_expr, NULL);
  code = g_list_append(code, g_strdup_printf(format, re->naming, starting_nodes, re->naming, re_expr_escaped, re->naming));
  free(re_expr_escaped);

  free(starting_nodes);

  return join_str_and_free(&code, "\n");
}

regex_t *regex_compile(const char *re_expr, const char *naming, const char *label) {
  regex_t *out = ALLOC(regex_t);
  out->errors = NULL;
  out->re_expr = g_strdup(re_expr);
  out->naming = g_strdup(naming);
  out->label = g_strdup(label);
  out->state = 0;

  GNode *tree = lex_tree_create(out->re_expr);
  if (!tree) {
    out->errors = g_list_append(out->errors, g_strdup("Basic parsing failed."));
    return out;
  }
  // lex_tree_print(tree, 0);

  GNode *form = lex_to_op(tree);
  if (!form) {
    out->errors = g_list_append(out->errors, g_strdup("Transformation to internal form failed."));
    return out;
  }

  lex_tree_destroy(tree);

  form = op_tree_collapse_concats(form);
  // op_print_tree(form, 0);
  out->internal_form = form;

  fa_t *nfa = regex_to_nfa(out);
  // fa_print(nfa);

  fa_t *dfa = fa_create_powerset(nfa);
  fa_destroy(nfa);
  // fa_print(dfa);

  gchar* code = regex_dfa_to_code(out, dfa);
  out->code = code;
  fa_destroy(dfa);

  if (!code) {
    out->errors = g_list_append(out->errors, g_strdup("Code generation failed!"));
    return out;
  }

  out->state = 1;

  return out;
}

void regex_destroy(regex_t *regex) {
  if (regex->internal_form) {
    op_tree_destroy(regex->internal_form);
  }
  if (regex->errors) {
    free_string_list(&regex->errors);
  }
  free(regex->re_expr);
  free(regex->naming);
  free(regex->label);
  if (regex->code) {
    free(regex->code);
  }
  free(regex);
}

int regex_module_c_add_regex(regex_module_c *self, regex_t * compiled_regex){
  self->errors = g_list_concat(self->errors, compiled_regex->errors);

  if( !compiled_regex->state ){
    return 0;
  }

  self->exprs = g_list_append(self->exprs, compiled_regex);
  return 1;
}

int regex_module_c_build(regex_module_c *self){
  char * user_env_cc = getenv("CC");
  if( !user_env_cc ){
    g_setenv("CC", "gcc", false);
  }

   const char * header =
    "#include <nativeextractor/miner.h>\n"
    "#include <nativeextractor/extractor.h>\n"
    "#include <nativeextractor/unicode.h>\n\n";

  free(self->code);
  self->code = g_strdup(header);

  size_t metalen = 2;
  size_t metacount = 0;
  char ** meta = malloc(sizeof(char *) * metalen);

  GList * node = self->exprs;
  while( node ){
    char * prev_code = self->code;
    regex_t * re = (regex_t*)node->data;

    self->code = g_strdup_printf("%s%s\n\n", prev_code, re->code);
    //printf("CODE: %s\n", self->code);
    free(prev_code);

    if (metacount == metalen) {
      metalen += 2;
      meta = realloc(meta, sizeof(char *) * metalen);
    }
    meta[metacount++] = g_strdup(re->naming);
    meta[metacount++] = g_strdup(g_strescape(re->re_expr, NULL));

    node = node->next;
  }

  {
    char * prev_code = self->code;
    self->code = g_strdup_printf("\n%s%s\n", prev_code, "const char *meta[] = {");
    free(prev_code);

    for (size_t i = 0; i < metacount; i += 2) {
      const char * fnname = meta[i];
      const char * label = meta[i + 1];
      prev_code = self->code;
      self->code = g_strdup_printf(
        "%s  \"%s\", \"%s\",\n",
        prev_code,
        fnname,
        label
      );
      free(prev_code);
    }

    prev_code = self->code;
    self->code = g_strdup_printf(
      "%s  NULL\n"
      "};\n",
      prev_code
    );
    free(prev_code);
  }

  for (size_t i = 0; i < metacount; ++i) {
    free(meta[i]);
  }
  free(meta);

  gchar * cname = g_strdup_printf("%s%lu_%s.c", self->build_path,
    (unsigned long)time(NULL), self->naming);
  gchar * soname = g_strdup_printf("%s%s.so", self->build_path, self->naming);

  FILE * fc = fopen(cname, "w+");
  fwrite(self->code, strlen(self->code), 1, fc);
  fclose(fc);

  gchar * cmd = g_strdup_printf("%s %s -o %s", REGEX_BUILD_CMD, cname, soname);
  //printf("CMD: %s\n", cmd);

  self->build_cmd = cmd;
  self->so_path = soname;

  int ret = system(cmd);

  free(cname);

  if( !user_env_cc ){
    g_unsetenv("CC"); /* Reset environment for sure */
  }

  if( ret != 0 ){
    char * err_str = g_strdup_printf("Compiler returned non-zero code: %d.", ret);
    fprintf(stderr, "%s", err_str);
    self->errors = g_list_append(self->errors, err_str);
    self->state = 0;
    return 0;
  }

  return 1;
}

int regex_module_c_load(regex_module_c * self, extractor_c *extractor){
  GList * regexes = self->exprs;
  bool ret = 1;

  while(regexes){
    regex_t * symb = regexes->data;

    ret &= extractor->add_miner_so(extractor, self->so_path,
      symb->naming, NULL);
    /* TODO: error handling*/

    if( !ret ){
      char * err = g_strdup_printf("Can not load: %s:%s\n", self->so_path, symb->naming);
      printf("%s", err);
      self->errors = g_list_append(self->errors, err);
    }

    regexes = regexes->next;
  }

  return ret;
}

void regex_module_c_destroy(struct regex_module_c * self){
  g_list_free_full(self->errors, free);
  g_list_free(self->exprs);

  free(self->naming);
  free(self->build_cmd);
  free(self->so_path);
  free(self->code);
  free(self);
}

regex_module_c * regex_module_c_new(const char * naming, const char * path) {
  regex_module_c * out = calloc(1, sizeof(regex_module_c));
  out->naming = g_strdup(naming);
  char * headers = getenv("REGEX_HEADER_FILES");
  out->headers_path = (headers ? headers : REGEX_HEADER_FILES);
  char * build = getenv("REGEX_BUILD_PATH");
  out->build_path = (char *)(path ? path : (build ? build : REGEX_BUILD_PATH));
  out->state = 1;

  out->add_regex = regex_module_c_add_regex;
  out->build = regex_module_c_build;
  out->load = regex_module_c_load;
  out->destroy = regex_module_c_destroy;

  return out;
}

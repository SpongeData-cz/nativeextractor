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

#ifndef REGEX_GENERATOR_H
#define REGEX_GENERATOR_H

#define REGEX_BUILD_PATH "/tmp/"
#define REGEX_HEADER_FILES "./src/"

#ifndef NE_PRODUCTION
  #define REGEX_BUILD_CMD "$CC $flags `pkg-config --cflags glib-2.0 python-2.7` " \
	  "`pkg-config --libs glib-2.0 python-2.7` " \
	  "-Iinclude/ -shared -fPIC"
#else
  #define REGEX_BUILD_CMD "$CC $flags `pkg-config --cflags glib-2.0 python-2.7 nativeextractor` " \
	  "`pkg-config --libs glib-2.0 python-2.7 nativeextractor` " \
	  "-Iinclude/ -shared -fPIC"
#endif

#include <glib-2.0/glib.h>
#include <nativeextractor/extractor.h>
#include <nativeextractor/finite_automaton.h>

typedef struct regex_t {
  /** Regex expression string. */
  char* re_expr;
  /** Regex unique naming specified by user */
  char* naming;
  /** Label tighten with the regex. */
  char* label;
  /** Reserved - tree for automaton construction. */
  GNode* internal_form;
  /** C99 generated code. */
  char* code;
  /** List of errors during regex compilation. */
  GList* errors;
  /** Boolean state 0 - fail, 1 - success. */
  int state;
} regex_t;

/**
 * Creates compiled regex_t structure. Must be freed by regex_destroy.
 *
 * @param re_expr Regular expression in stringual form.
 * @param naming Unique user-defined naming of the regular expression. Naming is used in generated function names.
 * @param label Label associated with this regex.
 *
 * @return A regex_t instance. For errors check .state and .errors.
 *  */
regex_t* regex_compile(const char* re_expr, const char* naming, const char* label);
/**
 * Frees a previously instantiated regex_t instance via regex_compile.
 *
 * @param regex Regex instance. */
void regex_destroy(regex_t *regex);

typedef struct regex_module_c {
  /** List of regexes. */
  GList *exprs;
  /** Module unique naming. */
  char *naming;
  /** Path to library destination. Default is REGEX_BUILD_PATH */
  char *build_path;
  /** Reserved path to headers important for build. Automaticaly derived from pkg-config now. */
  char *headers_path;
  /** Build command, by default REGEX_BUILD_CMD */
  char *build_cmd;
  /** Final generated code of the library. */
  char *code;
  /** Concatenation .build_path/.naming".so" */
  char *so_path;
  /** List of all errors. */
  GList * errors;
  /** Boolean state. */
  int state;
  /**
   *  Add regex to module.
   * @param self An instance of regex_module_c.
   * @param regex_t An instance of compiled regex.
   *
   * @return Boolean return state.
   */
  int (*add_regex)(struct regex_module_c *self, regex_t * compiled_regex);
  /**
   * Perform build of the module containing regexes. Only function that possibly modifies .state -> 0.
   * @param self An instance of regex_module_c.
   *
   * @return Boolean return state.
   */
  int (*build)(struct regex_module_c *self);
  /**
   * Apply add_miner_so() for every regex from the module to an extractor. Before calling .load, .build must be called first.
   * @param self An instance of regex_module_c.
   * @param extractor An instance of extractor_c.
   *
   * @return Boolean return state.
   */
  int (*load)(struct regex_module_c * self, extractor_c *extractor);
  /**
   * Frees memory used by the module.
   *
   * @param self An instance of regex_module_c. */
  void (*destroy)(struct regex_module_c * self);
} regex_module_c;

/**
 * Regex-module constructor.
 * Uses environmental variables REGEX_HEADER_FILES and REGEX_BUILD_PATH which defaults to equally named defines whenever not set.
 *
 * @param naming Unique naming of the module.
 * @param path Optional build path for the module. When NULL is passed environment REGEX_BUILD_PATH will be used or default to REGEX_BUILD_PATH.
*/
regex_module_c * regex_module_c_new(const char * naming, const char * path);

#endif  // REGEX_GENERATOR_H
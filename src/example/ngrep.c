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

#define _GNU_SOURCE

#include <nativeextractor/common.h>
#include <glib-2.0/glib.h>
#include <nativeextractor/extractor.h>
#include <nativeextractor/miner.h>
#include <nativeextractor/ner.h>
#include <nativeextractor/occurrence.h>
#include <nativeextractor/patricia.h>
#include <nativeextractor/stream.h>
#include <nativeextractor/unicode.h>
#include <nativeextractor/regex_generator.h>
#include <nativeextractor/unicode.h>
#include <nativeextractor/finite_automaton.h>
#include <nativeextractor/terminal.h>

#ifdef DEBUG
  #define LIBSTR "/usr/lib/MinersNonFree/"
#else
  #define LIBSTR "/usr/lib/MinersNonFree/"
#endif

#include <stdio.h>
#include <stdlib.h>

#define FMT_PLAIN 0
#define FMT_JSON 1
#define FMT_CSV 2


static gchar * a_expression = NULL;
static gchar * a_format = "plain";
static gchar * a_file = NULL; /* use g_free! */

static unsigned u_format = FMT_PLAIN;


gchar * rfc_escape_csv(char * str, unsigned len) {
  char * working_head = strdup(str);
  char * working = working_head;
  uint64_t offset = 0;

  while( *working ) {
    if( *working == '\"' ) {
      working_head = realloc(working_head, ++len);
      working = working_head + offset;
      memmove(working+1, working, len-offset-1);
      working++; // move twice xaxa"asdasd
      offset++;
    }

    working++;
    offset++;
  }

  return working_head;
}

void format_pos(occurrence_t * o) {
  char str[o->len + 1];
  str[o->len] = '\0';
  memcpy(str, o->str, sizeof(char) * o->len);
  if( u_format == FMT_PLAIN ) {
    printf("%s\n", str);
  }
  else if( u_format == FMT_JSON ) {
    printf("{\"pos\": %lu, \"len\": %u, \"val\": \"%s\"}\n", o->pos, o->len, str);
  }
  else if( u_format == FMT_CSV ) {
    gchar * esc = rfc_escape_csv(str, o->len+1);
    printf("\"%lu\",\"%u\",\"%s\"\n", o->pos, o->len, esc);
    free(esc);
  }
}

void analyze() {
  stream_file_c * sfc = stream_file_c_new(a_file);
  gchar * re_expr_enc = g_base64_encode(a_expression, strlen(a_expression));
  unsigned ree_len = strlen(re_expr_enc);
  while(re_expr_enc[ree_len-1] == '=') {
    re_expr_enc[ree_len-1] = '\0'; // remove trailing =
    ree_len--;
  }

  regex_t *re = regex_compile(a_expression, re_expr_enc, "REGEXP_MATCH");
  if (re->errors) {
    GList *err = re->errors;
    while (err) {
      PRINT_ERROR("%s", (char *)err->data);
      err = err->next;
    }
  }

  regex_module_c * so_module = regex_module_c_new(re_expr_enc, NULL);
  if( !so_module ) {
    fprintf(stderr, "Regex module creation failed\n.");
    exit(1);
  }

  so_module->add_regex(so_module, re);
  so_module->build(so_module);

  if (!so_module->build(so_module)) {
    GList *err = (GList*)so_module->errors;

    while (err) {
      PRINT_ERROR("%s", (char *)err->data);
      err = err->next;
    }
    exit(1);
  }

  miner_c ** miners = calloc(1, sizeof(miner_c*));
  extractor_c * e = extractor_c_new(1, miners);

  bool ret = so_module->load(so_module, e);
  if (!ret || !so_module->state || so_module->errors) {
    PRINT_ERROR("Module loading failed.");
    GList *err = (GList*)so_module->errors;

    while (err) {
      PRINT_ERROR("%s", (char *)err->data);
      err = err->next;
    }
    exit(1);
  }

  // Stream setting should go after miners addition
  if( !sfc || !e->set_stream(e, (stream_c*)sfc) ) {
    fprintf(stderr, "Stream set on Extractor failed.");
    exit(1);
  }

  uint32_t count = 0;

  while (!((e->stream->state_flags) & STREAM_EOF)) {
    occurrence_t ** res = e->next(e, 10000000);
    occurrence_t ** pres = res;

    while (*pres) {
      format_pos(*pres);
      free(*pres);
      ++pres;
      ++count;
    }

    free(res);
  }

  e->unset_stream(e);
  so_module->destroy(so_module);
  DESTROY(e);
  DESTROY(sfc);

  free(re_expr_enc);
}

static GOptionEntry entries[] =
{
  { "expression", 'e', 0, G_OPTION_ARG_STRING, &a_expression, "Regular Expression", "E" },
  { "format", 't', 0, G_OPTION_ARG_STRING, &a_format, "Output format - one of plain (dafault), ndjson, csv", "T" },
  { "file", 'f', 0, G_OPTION_ARG_FILENAME, &a_file, "Path to a file", "F" },
  { NULL }
};

int main (int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *context;

  context = g_option_context_new ("CONTEXT");
  g_option_context_add_main_entries (context, entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_print ("option parsing failed: %s\n", error->message);
    exit (1);
  }

  if( strcmp(a_format, "plain") == 0 ) {
    u_format = FMT_PLAIN;
  }
  else if( strcmp(a_format, "json") == 0 ) {
    u_format = FMT_JSON;
  }
  else if( strcmp(a_format, "csv") == 0 ) {
    u_format = FMT_CSV;
  }
  else {
    fprintf(stderr, "Bad output format. Use plain|json|csv only.`n");
    exit(1);
  }

  if( !a_file ) {
    fprintf(stderr, "No file given.\n");
    exit(1);
  }

  if( !a_expression ) {
    fprintf(stderr, "No expression given.\n");
    exit(1);
  }

  analyze();

  g_free(a_file);
  g_option_context_free(context);
  return 0;
}
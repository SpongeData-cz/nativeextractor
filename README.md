<p align="center"><img src="https://raw.githubusercontent.com/SpongeData-cz/nativeextractor/main/logo.svg" width="400" /></p>
<p align="center">
<a href="https://shields.io/"><img src="https://img.shields.io/badge/License-LGPLv3-blue.svg" alt="License LGPLv3"></a>
<a href="http://makeapullrequest.com"><img src="https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat-square" alt="PRs Welcome"></a>
<a href="https://GitHub.com/Naereen/StrapDown.js/graphs/commit-activity"><img src="https://img.shields.io/badge/Maintained%3F-yes-green.svg" alt="Maintenance"></a>
</p>

# NativeExtractor
NativeExtractor is a powerful tool that analyzes plaintext and extracts entities from it. Main features of NativeExtractor are:

 * High performance (multithreaded by default).
 * Support for unlimited plaintext inputs (2^48 Bytes as technical maximum).
 * Highly optimized implementation of Patty trie. Includes saving/ loading to/from an Mmapped file.
 * Module system and its dynamic loading.
 * RegExp compilation to native C `.so` modules - fastest RegExp you have ever seen!
 * Fast glob pattern recognition.
 * Bindings to Node.js and Python included.

# Table of Contents
- [NativeExtractor](#nativeextractor)
- [Table of Contents](#table-of-contents)
- [Getting started](#getting-started)
  - [Build process](#build-process)
  - [Usage](#usage)
- [Folder structure](#folder-structure)
- [Programming style](#programming-style)
  - [Coding conventions](#coding-conventions)
  - [Object oriented programming in C](#object-oriented-programming-in-c)
- [Extractor](#extractor)
  - [Flags](#flags)
- [Miners](#miners)
  - [Hello miner](#hello-miner)
  - [Building miners](#building-miners)
  - [Advanced miners](#advanced-miners)
  - [Testing miners](#testing-miners)
- [Glob miner](#glob-miner)
  - [Glob miner examples](#glob-miner-examples)
  - [Notes on glob miner implementation](#notes-on-glob-miner-implementation)
- [Native RegExps](#native-regexps)
  - [Native RegExps in practice](#native-regexps-in-practice)
- [Patty trie](#patty-trie)
  - [Example of use](#example-of-use)
- [Instant Examples](#instant-examples)
- [Other language bindings](#other-language-bindings)
- [Contributing](#contributing)
- [Special thanks](#special-thanks)

# Getting started
Compilation process is fully tested on Ubuntu 18.04 and 20.04, however other distros should run without complications. It is expectable that NativeExtractor should run on BSDs as well. MacOS is not supported.

**These are general requirements:**

 * A GNU/Linux distro or Docker
 * Makefiles
 * glib2.0 + development packages
 * python 2.7 + development package (python 3.0 planned soon)
 * node.js >=13 + development packages (optional)

**Dependencies installation on Ubuntu:**

```sh
sudo apt install build-essential libglib2.0-dev libpython2.7-dev libcmocka-dev
```

## Build process
```sh
make build
make install
```

Note that install script will install also headers into your `/usr/include`.

## Usage
You can simply use `pkg-config` to generate gcc/clang flags:

```sh
gcc main.c `pkg-config nativeextractor --libs --cflags` -o netest.bin
```

Declare following includes in your `main.c` file for full functionality, or see the docs for individual headers.

```c
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

...
```

# Folder structure
```
NativeExtractor/
|-- build/
|   |-- debug/            - Binaries built with config=debug (default)
|   |   `-- lib/          - Built entity miners (*.so)
|   |-- release/          - Binaries built with config=release
|   |   `-- lib/          - Built entity miners (*.so)
|-- include/
|   `-- nativeextractor/  - Header files
|-- src/                  - Source files
|   `-- miners/           - Place source codes of entity miners here for bult-in miners
|   `-- example/`         - Programmer-friendly examples to understand NativeExtractor basics
`-- Makefile
```

# Programming style

## Coding conventions
 * Exclusive usage of standard C11.
 * Indentation style in BSD KNF.
 * Block indentation uses 2 spaces.
 * Compilation should not produce warnings.
 * Every published function, struct etc. should be documented.

## Object oriented programming in C
Native extractor code is object-oriented however it is implemented in standard C. We use structs with methods as its variables. For example:

```c
#include <nativeextractor/common.h> // Includes macro ALLOC

/* C-style class definition. Names should end with _c postfix. */
typedef struct dog_c {
  unsigned weight;
  unsigned size;
  void (*woof)(struct dog_c* self);
} dog_c;

/* Implements method woof: */
void dog_c_woof(dog_c * self) {
  printf("I am unnamed dog, my weight is: %u and I am of size: %u\n",
    self->weight, self->size);
}

/* Initializes an instance of dog_c. */
void dog_c_init(dog_c * self, unsigned weight, unsigned size) {
  self->weight = weight;
  self->size = size;
  self->woof = dog_c_woof;
}

/* Creates a new instance of dog_c. */
dog_c * dog_c_new(unsigned weight, unsigned size) {
  dog_c * self = ALLOC(dog_c);
  dog_c_init(self, weight, size);
  return self;
}

dog_c * generic_dog = dog_c_new(50, 35);
generic_dog->woof(generic_dog); // Prints "I am unnamed dog, my weight is: 50..."

/* This class inherits from dog_c: */
typedef struct named_dog_c {
  dog_c dog; // Base class
  const char * name;
  void (*woof)(struct dog_c* self);
} named_dog_c;

/* Re-implement method woof: */
void named_dog_c_woof(named_dog_c * self) {
  printf("My name is %s dog, my weight is: %u and I am of size: %u\n",
    self->name, self->weight, self->size);
}

/* Initializes an instance of named_dog_c. */
void named_dog_c_init(named_dog_c * self, unsigned weight, unsigned size, const char * name) {
  dog_c_init(&self->dog, weight, size); // Initialize base class
  self->name = name;
  self->woof = named_dog_c_woof;
}

/* Creates a new instance of named_dog_c. */
named_dog_c * named_dog_c_new(unsigned weight, unsigned size, const char * name) {
  named_dog_c * self = ALLOC(named_dog_c);
  named_dog_c_init(self, weight, size, name);
  return self;
}

named_dog_c * named_dog = named_dog_c_new(50, 35, "Killer");

/* Now you can call re-implemented method woof: */
named_dog->woof(named_dog); // Prints "My name is Killer, ..."
/* Or method woof inherited from the base class: */
named_dog->dog.woof((dog_c*)named_dog); // Prints "I am unnamed dog, ..."
/* Or a bit longer way: */
((dog_c*)named_dog)->woof((dog_c*)named_dog); // Prints "I am unnamed dog, ..."
```

# Extractor
Extractor (`extractor_c`) is composed from these items:
 * Stream - an instance of `stream_c` created from a file or from (mapped) memory.
 * Marker - something like Turing-machine head operating on the stream.
 * List of miners - miners are small programs that accept position on the stream. Every miner have its own instance of a head for each invocation. Miners can move their head to the left or to the right and set markers.
 * Threads - every thread can be occupied by some number of miners. By default number of threads is equal to number of logical CPU cores.

Extractor does these operations (when calling `next()` method):
 * Executes each miner for the current position in the stream (in threads). A miner returns an instance of `occurrence_t` if a match is found at given position.
 * Accumulates found matches into a NULL-terminated array.
 * Moves head to the next UTF-8 character (from left to the right) in the stream.
 * Extractor does these operations repeatedly until it reaches end of the stream or a maximum size of a bulk.

## Flags
An extractor may have these flags enabled:
 * `E_SORT_RESULTS`
   * Sorts returned occurrences by position (ascending) and length (descending).
 * `E_NO_ENCLOSED_OCCURRENCES`
   * Filters out any enclosed occurrences.
   * An occurrence `A` is enclosed in occurrence `B` 
     if `A.start >= B.start` and `A.end <= B.end`.
   * For example:
     * `A`: `{start: 3, end: 6}`
     * `B`: `{start: 1, end: 9}`
     * `C`: `{start: 1, end: 7}`
     * `D`: `{start: 3, end: 9}`
     * Or as ASCII-art:
        ```
        A:  |--|
        B: |-------|
        C: |-----|
        D:  |------|
        ```
     * `A`, `C` and `D` are all enclosed in `B`.
     * Therefore, only `B` is returned.

To set or unset flags for an extractor, use the `set_flags` and `unset_flags` 
methods.
For example:
```c
extractor_c *ex = extractor_c_new(...);

ex->set_flags(ex, E_SORT_RESULTS | E_NO_ENCLOSED_OCCURRENCES);
// extractor now sorts results and discards enclosed occurrences

ex->unset_flags(ex, E_NO_ENCLOSED_OCCURRENCES);
// extractor no longer discards enclosed occurrences
```

# Miners
Each miner consists of at least two functions - one for creating its instances and one for matching occurrences.

When writing a miner you have two options - either you can include them directly in your main program, or you can create your own `.so` modules containing one or more miners and load them dynamically. Benefits of `.so` modules are:

 * Dynamic loading on runtime.
 * Building of miners is independent on extractor routines build.
 * Programmatic miner creation with native effectivity.
 * Usage in more binaries at a time.

When you decide to use the dynamic libraries, then all miner source files must be placed at the `src/miners` directory. One source file can contain definitions of multiple miners - if it makes sense. For example miners of credit card numbers from Visa, MasterCard etc. could be put into a single `card_entities.c` source file.

## Hello miner
Following is an example of a simple miner which finds all occurrences of "hello".

```c
// hello.c
#include <nativeextractor/miner.h>
#include <nativeextractor/unicode.h>

/** Matches string "hello". */
static occurrence_t* match_hello_impl(miner_c* m) {
  if (!m->mark_start(m)) return NULL;
  if (!m->match_string(m, "hello", Right)) return NULL;
  if (!m->mark_end(m)) return NULL;
  return m->make_occurrence(m, 1.0);
}

/** Returns a miner which matches a string "hello". */
miner_c* match_hello() {
  return miner_c_create("Hello", NULL, match_hello_impl);
}

/**
 * Metainfo for the shared library.
 * Format: [ "minerfn1", "label1", "minerfn2", "label2", ..., NULL ]
 */
const char* meta[] = {
  "match_hello", "Hello",
  NULL
};
```

For more info, have a look into the `miner.h` file, where you can find
documentation of all available miner methods.

## Building miners
To build a single miner directly from NativeExtractor, use the following command:

```sh
make miner name=source (config=debug/release)
```

Here the `source` is a name of the source file without the extension (`.c`). The
built miner can be found at `build/<config>/<source>.so`.

To build all existing miners, use:

```sh
make all-miners (config=debug/release)
```

To build miners from outside of NativeExtractor you have to run `make install` first and then run:

```sh
gcc -O0 -g3 -DDEBUG `pkg-config --cflags glib-2.0 nativeextractor`\
  `pkg-config --libs glib-2.0 nativeextractor` \
  -shared -fPIC \
  yourminer.c -o yourminer.so
```

## Advanced miners
Sometimes you would like to have configurable miners, especially in cases you want to have many similar miners differing in some parameter. We use this for example for a glob miner, where we pass the glob expression as its parameter. Look at the following fragment of the glob miner code.

```c
/* This miner entrypoint accepts glob as a parameter: */
miner_c* match_glob(const char* glob) {
  if (!is_glob(glob)) {
    fprintf(stderr, "'%s' is not a syntactically correct glob!\n", glob);
    return NULL;
  }
  /* create miner with data set to `glob` */
  return miner_c_create("Glob", glob, match_glob_impl);
}
```

And its usage:

```c
extractor->add_miner_so(extractor, "glob_entities.so", "match_glob", "hell*");
```

## Testing miners
When you've built your miner, you can test it by adding it into the `src/main.c`
file like so:

```c
// main.c
#include <nativeextractor/common.h>
#include <nativeextractor/extractor.h>

int main(int argc, char ** argv) {
  if (argc <= 1) {
    printf("Fullpath not specified!\n");
    return EXIT_FAILURE;
  }

  miner_c ** miners = calloc(1, sizeof(miner_c*));

  extractor_c * e = extractor_c_new(1, miners);
  // Add your miner here...
  e->add_miner_so(e, "./build/debug/lib/hello.so", "match_hello", NULL);
  stream_file_c * sfc = stream_file_c_new(argv[1]);
  e->set_stream(e, (stream_c*)sfc);

  while (!((e->stream->state_flags) & STREAM_EOF)) {
    occurrence_t ** res = e->next(e, 1000000);
    occurrence_t ** pres = res;
    while (*pres) {
      print_pos(*pres);
      free(*pres);
      ++pres;
    }
    free(res);
  }

  DESTROY(e);
  return EXIT_SUCCESS;
}
```

and then running:

```sh
make build && ./build/debug/NativeExtractor file.txt
```

# Glob miner
NativeExtractor includes an implementation of a glob miner. Our glob miner supports matching of:

 * **Exact strings** - matches only exactly equal strings, e.g. glob `hello` matches only string `hello` and nothing else.
 * **Wildcard operator ?** - `?` matches any single character, e.g. `hell?` matches `hello`, but also `hella`, `hellz`, `hell6` and any other single character at the end.
 * **Wildcard operator \*** - `*` works similarly to `?`, except it matches any number of characters, including 0. Glob `nat*tor` matches `nativextractor` for example, but only `nattor` as well.
 * **Character sets []** - e.g. `[abc]` matches character `a`, `b`, or `c`.
 * **Character ranges x-y** - are allowed within `[]` sets and it is a substitution for characters in the specified range. E.g. `[a-c]` would match character `a`, `b` or `c`.

## Glob miner examples
```c
extractor_c * ex;

// Common extractor_c initialization code here...

/*
 Add glob_entities to the extractor ex, note that path to the
 glob_entities.so may vary this glob matches all strings ending
 with .exe.
*/
ex->add_miner_so(extractor, "./build/release/lib/glob_entities.so",
  "match_glob", "*.exe");
/* Add glob *.bat to the extractor */
ex->add_miner_so(extractor, "./build/release/lib/glob_entities.so",
  "match_glob", "*.bat");
/* Add glob *.ini to the extractor */
ex->add_miner_so(extractor, "./build/release/lib/glob_entities.so",
  "match_glob", "*.ini");
```

## Notes on glob miner implementation
Glob miner interprets globs on the fly by using backtracking technique, so performance could be a little worse in comparison to native RegExps, but its main benefit is that you don't need to rebuild the `.so` module. For maximal performance, it should be possible to implement compilation of globs into native RegExps.

# Native RegExps
NativeExtractor implements fast RegExp matching - every RegExp is compiled into a native miner and then loaded, resulting into very high performance. This approach is also very useful for cases when input file is large or user expects large amount of files on input. Compilation process and loading of `.so` takes only a while and then it can be used unlimited number of times.

You can specify these environmental variables:
 * **CC** - compiler, by default gcc is used.
 * **REGEX_HEADER_FILES** - path to header files, by default `"./src"`.
 * **REGEX_BUILD_PATH** - target of output binaries, by default `"/tmp"`.

## Native RegExps in practice
```c
// Compile RegExp into native miner re_email
regex_t * re_email = regex_compile("[^@ \\t\\r\\n]+@[^@ \\t\\r\\n]+\\.[^@ \\t\\r\\n]+", "simple_email", "EMAIL");

// Typical check
if (re_email->state) {
  printf("Compilation terminated with these errors\n");
  /* We use glib library here */
  GList * errors = re_email->errors;
  for (; errors != NULL; errors = errors->next) {
    printf(errors->data);
  }
  exit(-1);
}

// Create a RegExp module.
regex_module_c * g_module = regex_module_c_new("testative", NULL);
// Add previously compiled RegExp re_email to regex module
g_module->add_regex(g_module, re_email);
// Build module and check its result
if (!g_module->build(g_module)) {
  /* handle errors here - errors are placed in ((GList*)g_module->errors) */
  exit(-1);
}

// Standard Initialization of extractor
miner_c ** miners = calloc(1, sizeof(miner_c*));
extractor_c * g_e = extractor_c_new(-1, miners);

// Load module as miner to the extractor g_e
g_module->load(g_module, g_e);

// Open a file as stream
stream_file_c * strf = stream_file_c_new("/tmp/some_file.txt");
// Set the stream to the extractor (must go after miner addition!)
g_e->set_stream(g_e, (stream_c*)strf);

// Find all occurrences of email till EOF not reached
while (!((g_e->stream->state_flags) & STREAM_EOF)) {
  // Analyze 1000 unicode chars and count results into res
  occurrence_t ** res = g_e->next(g_e, 1000);
  occurrence_t ** pres = res;

  // Iterate over results and free
  while (*pres) {
    // Print match
    print_pos(*pres);
    free(*pres);
    ++pres;
    ++count;
  }

  free(res);
}
```

# Patty trie
Patty trie is a highly optimized variant of [Radix tree](https://en.wikipedia.org/wiki/Radix_tree). We define Patty trie as Radix tree with count of edges limited by number of unicode characters. Patty trie works on UTF-8. Main properties of Patty trie are these:

 * Height - h of the trie is length of the longest string.
 * Width - w of the trie is number of unicode chars as maximum.
 * Search operation runs in O(log(n)).
 * Insert operation runs in O(log(n)).
 * Patty trie can be serialized into a file.
 * Serialized files can be loaded on the fly with Mmap, using all its benefits.
 * Implemented fast prefix matching and lookup.

## Example of use
```c
patricia_c* index = patricia_c_create(NULL);
index->insert(index, "Patrick", 0, 7);
index->insert(index, "Michael", 0, 7);
index->insert(index, "Paul", 0, 4);

uint32_t ret = g_pat->search(g_pat, "Patrick", 7);
if(ret == 7) {
  printf("Patrick FOUND\n");
}

index->save(index, "names.patty");
```
# Instant Examples
Programmer-friendly examples of use are included in `src/example` directory, full documentation is given. Build is done via `make examples`. Location of built example is `build/debug/` and must be run from project's `.` dir.

 We offer these examples:
  * *ngrep* - native grep tool compiling regexps to native code and executes that on a given file.
  * *glob* - interpretes a glob on a given file.
  * *naive_email_miner* - creates a simple miner with possibility to extract a subset of RFC-defined email adresses. It is built as simple console application and as a loadable .so module.

# Other language bindings
  * [Python](https://github.com/SpongeData-cz/pynativeextractor)
  * [Node.js](https://github.com/SpongeData-cz/nativeextractor.js)

# Contributing
NativeExtractor is produced by [SpongeData](https://spongedata.cz).

This project adheres to an universal code of conduct. By participating, you are expected to uphold this code.

Before you send a pull request, search for previous discussions about the same feature or issue.

When contributing code, make sure build process is functional and hold [Programming style](#programming-style).

Before sending a pull request, be sure to have tests for meaningful cases (new features, bugs, ...).

# Special thanks
We thank to NativeExtractor logo creator - [Adam Říha](https://adamriha.com/) from [Gaupi](https://gaupi.cz).

We also thank to our code contributors.
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

#ifndef EXTRACTOR_H
#define EXTRACTOR_H
#include <pthread.h>
#include <semaphore.h>

#include <nativeextractor/common.h>
#include <nativeextractor/miner.h>
#include <nativeextractor/occurrence.h>
#include <nativeextractor/stream.h>

#define DEFAULT_THREADS 8 // TODO: compute best threads

typedef struct dl_symbol_t {
  /** Path to the .so library. */
  const char * ldpath;
  /** Miner function name. */
  const char * ldsymb;
  /** Meta info about miner functions and labels. */
  const char ** meta;
  /** Miner params. */
  const char * params; // FIXME: Shouldn't this be void*?
  /** Pointer to the loaded .so library. */
  void * ldptr;
} dl_symbol_t;

typedef struct thread_args_t {
  miner_c* miner;
  occurrence_t*** pout;
  unsigned batch;
} thread_args_t;

typedef struct extractor_c {
  /**
   * Analyzes next batch with miners.
   *
   * @param self  self pointer
   * @param batch number of logical symbols to be analyzed in the stream
   * @return      NULL-terminated array of occurencies (need to be correctly freed by user)
   */
  occurrence_t** (*next)(struct extractor_c * self, unsigned batch);

  /**
   * Set stream to extract on.
   *
   * @param self     self pointer
   * @param stream   instance of a stream_c
   * @return         true if opening was successful, false otherwise
   */
  bool (*set_stream)(struct extractor_c * self, stream_c * stream);

  /**
   * Unset stream to NULL.
   *
   * @param self an extractor_c instance
   */
  void (*unset_stream)(struct extractor_c* self);

  /**
   * Adds miner placed in a .so library to miner array dynamicaly
   *
   * @param self    self pointer
   * @param so_dir  path to a .so library
   * @param symb    symbol (name) of a function in the .so library that returns
   *                an instance of a miner
   * @param params  parameters for the miner (eg. a glob pattern)
   *
   * @returns       true if symbol has inferred correct miner_c, false otherwise
   */
  bool (*add_miner_so)(struct extractor_c * self, const char * so_dir, const char * symb, void * params);

  void (*set_last_error)(struct extractor_c * self, const char * err);

  const char * (*get_last_error)(struct extractor_c * self);

  /**
   * Destroys itself, closes attached stream and closing opened libraries
   *
   * @returns       void
   */
  void (*destroy)(struct extractor_c* self);

  /**
   * List of miners
   */
  miner_c ** miners;
  /**
   * Count of miners in the miners array
   */
  unsigned miners_count;
  /**
   * Limit of threads to run miners on (currently always 1)
   */
  unsigned threads_count;
  /**
   * A NULL-terminated array of dl_symbol_t instances to track imported miners
   */
  dl_symbol_t ** dlsymbols;
  /**
   * A stream_c instance to work on
   */
  stream_c * stream;
  char * last_error;

  pthread_t * threads;
  sem_t sem_main;
  pthread_mutex_t mutex_pout;// = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t mutex_extractor;

  sem_t sem_targs;
  pthread_mutex_t mutex_targs;// = PTHREAD_MUTEX_INITIALIZER;
  thread_args_t** targs; // TODO: Free!
  size_t targs_count;// = 0;
  bool threads_inited;
  bool terminate_p;
} extractor_c;

extractor_c * extractor_c_new(int threads, miner_c ** miners);

#endif // EXTRACTOR_H

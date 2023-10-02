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

#include <nativeextractor/extractor.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>

/*! \mainpage NativeExtractor documentation
 *
 * \section welcome_section Welcome to the NativeExtractor documentation
 *
 * For further details please see [README.md](README.md).
 *
 * For full documentation please navigate through the menu on the left.
 */

void* thread_fn(void* args) {
  mark_t mark;
  extractor_c * extractor = (extractor_c*)args;

  while (true) {
    sem_wait(&(extractor->sem_targs));

    if (extractor->terminate_p) {
      sem_post(&(extractor->sem_main));
      break;
    }

    pthread_mutex_lock(&(extractor->mutex_targs));
    thread_args_t* targs = extractor->targs[--(extractor->targs_count)];
    pthread_mutex_unlock(&(extractor->mutex_targs));

    miner_c* miner = targs->miner;
    int64_t batch = (int64_t)targs->batch;

    while (!(miner->stream->state_flags & STREAM_EOF) && batch > 0) {
      // Check if:
      //  * Current position in then stream farther than in the last run
      //  * Current position in then stream farther than the last matched occurrence
      if (miner->stream->pos >= MAX(miner->pos_last, miner->end_last)) {
        miner->mark_pos(miner, &mark);
        occurrence_t* p = miner->run(miner);

        if (p) {
          // add occurrence
          size_t last_pos = p->pos + p->len;

          pthread_mutex_lock(&(extractor->mutex_pout));

          if (extractor->flags & E_NO_ENCLOSED_OCCURRENCES) {
            if ((extractor->last_max > 0)
                && (last_pos <= extractor->last_max)) {
              // skip enclosed occurrence
              free(p);
              goto exit_mutex;
            }
          }

          **(targs->pout) = p;
          ++(*(targs->pout));

exit_mutex:
          pthread_mutex_unlock(&(extractor->mutex_pout));
        }

        if (miner->stream->unicode_offset > mark.unicode_offset) {
          batch -= (miner->stream->unicode_offset - mark.unicode_offset - 1);
          miner->stream->move(miner->stream, -1);
        } else {
          miner->reset_pos(miner, &mark);
        }
      }
      batch -= miner->stream->move(miner->stream, 1);
    }

    sem_post(&(extractor->sem_main));
    free(targs);
  }

  pthread_exit(NULL);
}

int occurrence_t_compare(const occurrence_t * o1, const occurrence_t * o2){
  if( o1->pos > o2->pos )
    return 1;
  if( o1->pos < o2->pos )
    return -1;
  if(o1->len < o2->len)
    return -1;
  if(o1->len > o2->len)
    return 1;
  return 0;
}

void occurrence_t_sort(occurrence_t ** batch) {
  unsigned o_count = 0;
  occurrence_t ** pbatch = batch;

  while(*(pbatch++)) {
    o_count++;
  }

  qsort(batch, o_count, sizeof(occurrence_t*), (__compar_fn_t)occurrence_t_compare);
}

/**
 * Destructively removes "enclosed "occurrences from passed array and returns
 * pointer to filtered occurrences.
 * The passed array may no longer be valid after this function call and
 * the return value should be used for subsequent use of the original array.
 *
 * Example:
 * Occurrences are shown as spans [pos:pos + len].
 * <pre>
 * A: |----------|      // not enclosed in any occurrence, to be kept
 * B: |---|             // enclosed in A, to be removed
 * C:      |----|       // enclosed in A and E, to be removed
 * D:   |------|        // enclosed in A, to be removed
 * E:    |----------|   // not enclosed in any occurrence, to be kept
 * </pre>
 *
 * @param occurrences the occurrences to filter
 *
 * @return filtered occurrences; use in place of passed array
 */
occurrence_t **filter_longest_occurrences(occurrence_t **occurrences) {
  occurrence_t **pa;
  occurrence_t **pb;

  for (pa = occurrences; *pa != NULL; ++pa) {
    for (pb = pa + 1; *pb != NULL; ++pb) {
      occurrence_t *a = *pa;
      occurrence_t *b = *pb;
      size_t aend = a->pos + a->len;
      size_t bend = b->pos + b->len;

      if (a->label == NULL) {
        // already filtered out
        break;
      }

      if (b->label == NULL) {
        // already filtered out
        continue;
      }

      if ((a->pos == b->pos) && (a->len == b->len)) {
        // keep identical occurrences with different labels
        continue;
      }

      if ((a->pos <= b->pos) && (bend <= aend)) {
        b->label = NULL; // mark for deletion
      }

      if ((b->pos <= a->pos) && (aend <= bend)) {
        a->label = NULL; // mark for deletion
      }
    }
  }

  // shrink array to not contain occurrences marked for deletion
  size_t found = 0;
  /** true if array needs to be shrunk */
  bool freed = false;
  pa = pb = occurrences;
  while (*pb != NULL) {
    occurrence_t *occ = *pb;
    if (occ->label != NULL) {
      *pa = *pb;
      ++pa;
      ++found;
    } else {
      freed = true;
      free(occ);
    }
    ++pb;
  }
  *pa = NULL;

  return freed
      ? realloc(occurrences, (found + 1) * sizeof (occurrence_t *))
      : occurrences;
}

occurrence_t** next(extractor_c * self, unsigned batch) {
  pthread_mutex_lock(&(self->mutex_extractor));
  if (!self->threads_inited) {
    sem_init(&(self->sem_main), 0, 0);
    free(self->targs);
    self->targs = calloc(self->miners_count, sizeof(thread_args_t*));
    self->threads_inited = true;
  }

  occurrence_t** out = malloc(((batch * self->miners_count) + 1) * sizeof(occurrence_t*));
  occurrence_t** pout = out;

  for (unsigned m = 0; m < self->miners_count; ++m) {
    miner_c * miner = self->miners[m];
    miner->stream->sync(miner->stream, self->stream);

    thread_args_t* targs = ALLOC(thread_args_t);
    targs->miner = miner;
    targs->batch = batch;
    targs->pout = &pout;

    pthread_mutex_lock(&(self->mutex_targs));
    self->targs[self->targs_count++] = targs;
    pthread_mutex_unlock(&(self->mutex_targs));

    sem_post(&(self->sem_targs));
  }

  self->stream->move(self->stream, (int64_t)batch);

  PRINT_DEBUG("Waiting!\n");
  for (unsigned m = 0; m < self->miners_count; ++m) {
    PRINT_DEBUG("%u / %u finished!\n", m + 1, self->miners_count);
    sem_wait(&(self->sem_main));
  }

  *pout = NULL;

  if (self->flags & E_NO_ENCLOSED_OCCURRENCES) {
    out = filter_longest_occurrences(out);
    size_t max_pos = self->last_max;
    occurrence_t **occ = out;
    while (*occ != NULL) {
      max_pos = MAX(max_pos, (*occ)->pos + (*occ)->len);
      ++occ;
    }
    self->last_max = max_pos;
  }

  pthread_mutex_unlock(&(self->mutex_extractor));

  if( self->flags & E_SORT_RESULTS ) {
    occurrence_t_sort(out);
  }

  return out;
}

bool extractor_c_set_stream(extractor_c * self, stream_c * stream){
  self->unset_stream(self);

  pthread_mutex_lock(&(self->mutex_extractor));
  self->stream = stream;

  int err;
  int flags = self->stream->state_flags;

  if (flags & STREAM_FAILED) {
    pthread_mutex_unlock(&(self->mutex_extractor));
    self->set_last_error(self, strerror(errno));
    return false;
  }

  for (unsigned m = 0; m < self->miners_count; ++m) {
    self->miners[m]->set_stream(self->miners[m], self->stream);
  }

  self->last_max = 0;

  pthread_mutex_unlock(&(self->mutex_extractor));

  return true;
}

void extractor_c_unset_stream(extractor_c* self) {
  pthread_mutex_lock(&(self->mutex_extractor));
  self->stream = NULL;

  self->threads_inited = false;
  sem_close(&(self->sem_main));
  free(self->targs);
  self->targs = NULL;
  self->targs_count = 0;

  pthread_mutex_unlock(&(self->mutex_extractor));
}

static inline void dl_symbol_t_print(dl_symbol_t* dls) {
  printf("dl_symbol_t { ldpath: \"%s\", ldsymb: \"%s\", params: %p, ldptr: %p }\n",
    dls->ldpath, dls->ldsymb, dls->params, dls->ldptr);
}

bool extractor_c_add_miner_from_so(extractor_c * self,
  const char * miner_so_path, const char * miner_name, void * params ){
    pthread_mutex_lock(&(self->mutex_extractor));

    dl_symbol_t ** dls = self->dlsymbols;
    void * dlfound_p = NULL;
    bool symbfound_p = false;
    unsigned dls_count = 0;

    while(*dls){
      if( !dlfound_p && strcmp((*dls)->ldpath, miner_so_path) == 0 ){
        dlfound_p = (*dls)->ldptr;
      }

      if(dlfound_p){
        if( !symbfound_p && strcmp((*dls)->ldsymb, miner_name) == 0){
          symbfound_p = true;
        }
      }

      if( dlfound_p && symbfound_p && !params ){
        // Already found
        // fprintf(stderr, "WARNING: Symbol already present: %s\n", miner_name);
        pthread_mutex_unlock(&(self->mutex_extractor));
        return true;
      }

      dls++;
      dls_count++;
    }

    if( !dlfound_p ){
      dlfound_p = dlopen(miner_so_path, RTLD_LAZY);

      if( dlfound_p == NULL ){
        pthread_mutex_unlock(&(self->mutex_extractor));
        self->set_last_error(self, dlerror());
        return false;
      }
    }

    if( !symbfound_p || params ){
      miner_c* (*miner_new)(const void*) = dlsym(dlfound_p, miner_name);
      if( !miner_new ){
        pthread_mutex_unlock(&(self->mutex_extractor));
        self->set_last_error(self, dlerror());
        return false;
      }

      const char** meta = dlsym(dlfound_p, "meta");
      size_t metalen;
      for (metalen = 0; meta[metalen] != NULL; ++metalen) {};

      /* realloc so objects array */
      dl_symbol_t* ns = malloc(sizeof(dl_symbol_t));
      ns->ldpath = strdup(miner_so_path);
      ns->ldsymb = strdup(miner_name);
      ns->params = params;


      ns->meta = malloc((metalen + 1) * sizeof(char*));
      for (size_t i = 0; i < metalen; ++i) {
        ns->meta[i] = strdup(meta[i]);
      }
      ns->meta[metalen] = NULL;
      ns->ldptr = dlfound_p;

      /* add to the end of .dlsymbols */
      *dls = ns;

      self->dlsymbols = realloc(self->dlsymbols, (dls_count +2) * sizeof(dl_symbol_t*));
      self->dlsymbols[dls_count+1] = NULL; // termination

      /* realloc miners array */
      miner_c* miner = miner_new(params);

      ++self->miners_count;
      self->miners = realloc(self->miners, sizeof(miner_c*) * (self->miners_count + 1));

      self->miners[self->miners_count - 1] = miner;
      self->miners[self->miners_count] = NULL;

      pthread_mutex_lock(&(self->mutex_targs));
      self->targs = realloc(self->targs, sizeof(thread_args_t*)*self->miners_count);
      pthread_mutex_unlock(&(self->mutex_targs));

      pthread_mutex_unlock(&(self->mutex_extractor));

      return true;
    }

    pthread_mutex_unlock(&(self->mutex_extractor));
    assert(0);
    return false;
}

void extractor_c_destroy(extractor_c * self){
  self->unset_stream(self);

  pthread_mutex_lock(&(self->mutex_extractor));

  self->terminate_p = true;
  for (size_t i = 0; i < self->threads_count; ++i) {
    sem_post(&(self->sem_targs));
  }

  for (size_t i = 0; i < self->threads_count; ++i) {
    sem_wait(&(self->sem_main));
  }

  /* threads down now */

  // Destroy miners
  for (unsigned m = 0; m < self->miners_count; ++m) {
    DESTROY(self->miners[m]);
  }
  free(self->miners);

  // Destroy loaded .so
  dl_symbol_t ** dls = self->dlsymbols;

  while (*dls) {
    void * ldptr = (*dls)->ldptr;
    dl_symbol_t ** dls_end = dls;

    if (ldptr) { /* non-null library pointer detected */
      while (*dls_end) {
        if ((*dls_end)->ldptr == ldptr) {
          (*dls_end)->ldptr = NULL; /* zero ldptr to not close only once */
        }
        ++dls_end;
      }

      if (ldptr) {
        dlclose(ldptr);
      }
    }

    free((char*)(*dls)->ldpath);
    free((char*)(*dls)->ldsymb);
    for (size_t i = 0; (*dls)->meta[i] != NULL; ++i) {
      free((char*)(*dls)->meta[i]);
    }
    free((char**)(*dls)->meta);
    free(*dls);
    ++dls;
  }

  pthread_mutex_destroy(&(self->mutex_targs));
  pthread_mutex_destroy(&(self->mutex_pout));

  // free threading
  free(self->threads);

  for( unsigned tc = 0 ; tc < self->targs_count; ++tc ){
    free( self->targs[tc] );
  }

  free(self->targs);

  // free
  free(self->dlsymbols);
  free(self->last_error);

  pthread_mutex_destroy(&(self->mutex_extractor));
}

const char * extractor_get_last_error(extractor_c * self) {
  return (self->last_error ? self->last_error : "");
}

void extractor_set_last_error(extractor_c * self, const char * err) {
  pthread_mutex_lock(&(self->mutex_extractor));
  free(self->last_error);
  self->last_error = strdup(err);
  pthread_mutex_unlock(&(self->mutex_extractor));
}

/**
 * Set or unset flags.
 *
 * @param self the extractor
 * @param flags the flags
 * @param value true or false
 *
 * @returns true on success
 */
bool _set_flags(extractor_c * self, unsigned flags, bool value) {
  // only allow defined flags
  if (flags & ~(E_NO_ENCLOSED_OCCURRENCES | E_SORT_RESULTS)) {
    return false;
  }

  // NOTE: check flag interference here

  self->flags = value
      ? self->flags | flags
      : self->flags & ~flags;

  return true;
}

bool extractor_set_flags(extractor_c * self, unsigned flags) {
  return _set_flags(self, flags, true);
}

bool extractor_unset_flags(extractor_c * self, unsigned flags) {
  return _set_flags(self, flags, false);
}

extractor_c * extractor_c_new(int threads, miner_c ** miners){
  extractor_c * out = calloc(1, sizeof(extractor_c));
  out->threads_count = (threads < 1 ? sysconf(_SC_NPROCESSORS_ONLN) : threads);
  out->miners = miners;

  unsigned miners_count = 0;
  while (miners && miners[miners_count] != NULL) { ++miners_count; }

  out->next = next;
  out->set_stream = extractor_c_set_stream;
  out->unset_stream = extractor_c_unset_stream;
  out->add_miner_so = extractor_c_add_miner_from_so;
  out->miners_count = miners_count;
  out->dlsymbols = calloc(1, sizeof(dl_symbol_t*)); /* NULL terminated array*/
  out->destroy = extractor_c_destroy;
  out->get_last_error = extractor_get_last_error;
  out->set_last_error = extractor_set_last_error;
  out->set_flags = extractor_set_flags;
  out->unset_flags = extractor_unset_flags;

  out->stream = NULL;//stream_c_new();
  out->last_error = NULL;
  out->targs_count = 0;
  out->terminate_p = false;
  out->last_max = 0;

  pthread_mutex_init(&(out->mutex_pout), NULL);
  pthread_mutex_init( &(out->mutex_targs), NULL);
  pthread_mutex_init( &(out->mutex_extractor), NULL);
  sem_init(&(out->sem_targs), 0, 0);

  out->threads = calloc(out->threads_count, sizeof(pthread_t));

  for (unsigned m = 0; m < out->threads_count; ++m) {
    while (1) {
      int result_code = pthread_create(&(out->threads[m]), NULL, thread_fn, out);
      if (result_code == EAGAIN) { // retry thread creation if system resources are missing
        usleep(333);
        continue;
      }

      if (!result_code) {
        break;
      }

      assert(!result_code);
    }
  }

  PRINT_DEBUG("Initialized!\n");
  out->threads_inited = false;

  return out;
}

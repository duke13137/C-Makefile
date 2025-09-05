#include <stdio.h>
#include "arena.h"
#include "cc.h"
#include "debug.h"

static inline void *vt_arena_malloc(size_t size, Arena **ctx) {
  return arena_malloc(size, *ctx);
}

static inline void vt_arena_free(void *ptr, size_t size, Arena **ctx) {
  arena_free(ptr, size, *ctx);
}

static inline size_t astr_wyhash(astr key) {
  return cc_wyhash(key.data, key.len);
}

#define NAME      Map_astr_astr
#define KEY_TY    astr
#define VAL_TY    astr
#define CTX_TY    Arena *
#define CMPR_FN   astr_equals
#define HASH_FN   astr_wyhash
#define MALLOC_FN vt_arena_malloc
#define FREE_FN   vt_arena_free
#include "verstable.h"

void test_vt(Arena *arena) {
  ALOG(arena);

  Map_astr_astr mymap;
  vt_init_with_ctx(&mymap, arena);

  for (int i = 0; i < 10; ++i) {
    astr k = astr_format(arena, "key-%d", i);
    astr v = astr_format(arena, "%d", 10000 + i);
    vt_insert(&mymap, k, v);
  }

  for (int i = 0; i < 100; ++i) {
    astr k = astr_format(arena, "key-%d", i);
    Map_astr_astr_itr it = vt_get(&mymap, k);
    if (vt_is_end(it)) {
      break;
    }
    printf("%.*s found %.*s!\n", S(it.data->key), S(it.data->val));
  }

  ALOG(arena);
  vt_cleanup(&mymap);
  ALOG(arena);
}

#define BGEN_NAME   max_priority_queue
#define BGEN_TYPE   int
#define BGEN_LESS   return b < a;
#define BGEN_MALLOC return arena_malloc(size, udata);
#define BGEN_FREE   arena_free(ptr, size, udata);
#include "bgen.h"

#define BGEN_NAME   min_priority_queue
#define BGEN_TYPE   int
#define BGEN_LESS   return a < b;
#define BGEN_MALLOC return arena_malloc(size, udata);
#define BGEN_FREE   arena_free(ptr, size, udata);
#include "bgen.h"

int test_pqueue(Arena *arena) {
  {
    Scratch(arena);
    int data[] = {1, 8, 5, 6, 3, 4, 0, 9, 7, 2};
    int n = sizeof(data) / sizeof(int);
    printf("\ndata: ");
    for (int i = 0; i < n; i++) {
      printf("%d ", data[i]);
    }
    printf("\n");

    struct max_priority_queue *max_priority_queue = 0;

    // Fill the priority queue.
    for (int i = 0; i < n; i++) {
      max_priority_queue_insert(&max_priority_queue, data[i], 0, arena);
    }

    printf("max_priority_queue: ");
    while (max_priority_queue_count(&max_priority_queue, 0) > 0) {
      int val = 0;
      max_priority_queue_pop_front(&max_priority_queue, &val, arena);
      printf("%d ", val);
    }
    printf("\n");

    struct min_priority_queue *min_priority_queue = 0;

    // Fill the priority queue.
    for (int i = 0; i < n; i++) {
      min_priority_queue_insert(&min_priority_queue, data[i], 0, arena);
    }

    printf("min_priority_queue: ");
    while (min_priority_queue_count(&min_priority_queue, 0) > 0) {
      int val = 0;
      min_priority_queue_pop_front(&min_priority_queue, &val, arena);
      printf("%d ", val);
    }
    printf("\n");
  }
  return 0;
}

// No need to define a hash, comparison, or destructor function for CC strings here as these
// functions are defined by default.

int test_string(void) {
  int err = 0;
  map(str(char), str(char)) our_map;
  init(&our_map);

  // Regular insertion of CC strings.
  str(char) our_str_key;
  str(char) our_str_el;
  init(&our_str_key);
  init(&our_str_el);

  if (!push_fmt(&our_str_key, "France") || !push_fmt(&our_str_el, "Paris") ||
      !insert(&our_map, our_str_key, our_str_el)) {
    // Out of memory, so abort.
    // This requires cleaning up the strings, too, since they were not inserted and the map therefore
    // did not take ownership of them.
    cleanup(&our_str_key);
    cleanup(&our_str_el);
    err = -1;
    goto CLEANUP;
  }

  // Heterogeneous insertion of C strings.
  // CC automatically creates CC strings (and cleans them up if the operation fails).
  if (!insert(&our_map, "Japan", "Tokyo")) {
    err = -1;
    goto CLEANUP;
  }

  str(char) our_str_lookup_key;
  init(&our_str_lookup_key);

  // Regular lookup using a CC string.
  if (!push_fmt(&our_str_lookup_key, "Japan")) {
    err = -1;
    goto CLEANUP;
  }
  str(char) *el = get(&our_map, our_str_lookup_key);
  char *s = first(el);
  puts(s);

  // Heterogeneous lookup using a C string.
  // This requires no dynamic memory allocations.
  el = get(&our_map, "France");
  puts(first(el));

CLEANUP:
  cleanup(&our_str_lookup_key);
  cleanup(&our_map);

  return err;
}

#include <stdio.h>
#include "arena.h"
#include "cc.h"
#include "debug.h"
#include "utest.h"

static inline void* vt_arena_malloc(size_t size, Arena** ctx) {
  return arena_malloc(size, *ctx);
}

static inline void vt_arena_free(void* ptr, size_t size, Arena** ctx) {
  arena_free(ptr, size, *ctx);
}

static inline size_t astr_wyhash(astr key) {
  return cc_wyhash(key.data, key.len);
}

#define NAME      Map_astr_astr
#define KEY_TY    astr
#define VAL_TY    astr
#define CTX_TY    Arena*
#define CMPR_FN   astr_equals
#define HASH_FN   astr_wyhash
#define MALLOC_FN vt_arena_malloc
#define FREE_FN   vt_arena_free
#include "verstable.h"

Map_astr_astr test_vt(Arena* arena) {
  ALOG(arena);

  Map_astr_astr mymap;
  vt_init_with_ctx(&mymap, arena);

  for (int i = 0; i < 10; ++i) {
    astr k = astr_format(arena, "key-%d", i);
    astr v = astr_format(arena, "%d", 10000 + i);
    vt_insert(&mymap, k, v);
  }

  ALOG(arena);
  return mymap;
}

UTEST(Map, astr) {
  enum { size = KB(1) };
  byte mem[size] = {0};
  Arena arena[] = {arena_init(mem, size)};
  Map_astr_astr mymap = test_vt(arena);

  for (int i = 0; i < 100; ++i) {
    astr k = astr_format(arena, "key-%d", i);
    Map_astr_astr_itr it = vt_get(&mymap, k);
    if (vt_is_end(it)) {
      break;
    }
    printf("%.*s found %.*s!\n", S(it.data->key), S(it.data->val));
  }
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

int test_pqueue(Arena a) {
  Arena arena[] = {a};

  int data[] = {1, 8, 5, 6, 3, 4, 0, 9, 7, 2};
  int n = sizeof(data) / sizeof(int);
  printf("data: ");
  for (int i = 0; i < n; i++) {
    printf("%d ", data[i]);
  }
  printf("\n");

  struct max_priority_queue* max_priority_queue = 0;

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

  struct min_priority_queue* min_priority_queue = 0;

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
  return 0;
}

#if defined(LOGGING) && defined(__linux__)
#define UPRINTF_IMPLEMENTATION
#include "uprintf.h"
#endif

#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include "arena.h"
#include "debug.h"
#include "utest.h"
UTEST_STATE();

/* --- Default Thread-Local Arena --- */

#ifndef DEFAULT_ARENA_SIZE
#define DEFAULT_ARENA_SIZE MB(64)
#endif

// Thread-local default arena
__thread Arena* default_arena = NULL;

#ifndef OOM_COMMIT
__thread void* default_arena_mem = NULL;
#endif

/**
 * Get the default arena, initializing it on first use.
 *
 * Usage:
 *   Arena *a = arena_default();
 *   char *s = New(a, char, 100);
 */
static inline Arena* arena_default(void) {
  if (default_arena) {
    return default_arena;
  }

#ifdef OOM_COMMIT
  static __thread Arena storage = {0};
  storage = arena_init(NULL, 0);
  default_arena = &storage;
#else
  default_arena_mem = malloc(DEFAULT_ARENA_SIZE);
  if (!default_arena_mem) {
    perror("arena_default malloc");
    abort();
  }

  static __thread Arena storage = {0};
  storage = arena_init(default_arena_mem, DEFAULT_ARENA_SIZE);
  default_arena = &storage;
#endif

  return default_arena;
}

/**
 * Reset the default arena to reclaim all memory.
 * Call this when you're done with temporary allocations.
 *
 * Usage:
 *   Arena *a = arena_default();
 *   // ... do work ...
 *   arena_default_reset();
 */
static inline void arena_default_reset(void) {
  if (default_arena) {
    arena_reset(default_arena);
  }
}

/**
 * Save the current state of the default arena.
 * Returns a snapshot that can be restored later.
 *
 * Usage:
 *   Arena snapshot = arena_default_snapshot();
 *   // ... temporary allocations ...
 *   arena_default_restore(snapshot);
 */
static inline Arena arena_default_snapshot(void) {
  Arena* a = arena_default();
  return *a;
}

/**
 * Restore the default arena to a previous snapshot.
 * Frees all allocations made after the snapshot.
 *
 * Usage:
 *   Arena snap = arena_default_snapshot();
 *   char *temp = New(arena_default(), char, 100);
 *   arena_default_restore(snap);  // temp is now invalid
 */
static inline void arena_default_restore(Arena snapshot) {
  if (default_arena) {
    ASAN_POISON_MEMORY_REGION(snapshot.cur, default_arena->end - snapshot.cur);
    default_arena->cur = snapshot.cur;
  }
}

astr test_astr(Arena arena[static 1]) {
  {
    // Scratch(arena);

    ALOG(arena);
    astr s3 = {0};

    ALOG(arena);

    astr s = {0};
    s = astr_clone(arena, s);
    s = astr_clone(arena, astr(""));
    s = astr_cat_cstr(arena, s, "hello");
    astr s1 = astr_format(arena, "%.10f, $%d, %.*s", 3.1415926, 42, S(s));
    printf("test_astr: %.*s\n", S(s1));
    char buf[] = ", world, \0!!!   ";
    s3 = astr_cat_bytes(arena, s1, buf, sizeof(buf));
    printf("test_astr: %.*s\n", S(s3));

    printf("test_astr: %s\n", astr_to_cstr(*arena, s3));
    int i = 0;
    for (astr_split(it, ",", s3)) {
      printf("|%.*s|\n", S(astr_trim(it.token)));
      i++;
    }
    printf("num of token=%d\n", i);

    i = 0;
    for (astr_split_by_char(it, ",| $", s3, arena)) {
      printf("'%s'\n", astr_to_cstr(*arena, astr_slice(it.token, 1, 10)));
      i++;
    }
    printf("num of token=%d\n", i);

    ALOG(arena);
    return s3;
  }
}

typedef slice(int64_t) i64s;

i64s test_slice(Arena* arena) {
  {
    Scratch(arena);
    slice(int) fibs = {0};
    fibs = Clone(arena, fibs);
    *Push(&fibs, arena) = 2;
    *Push(&fibs, arena) = 3;
  }

  int64_t data[] = {2, 3, 42};
  i64s fibs = {.data = data, .len = countof(data)};
  fibs = Clone(arena, fibs, 0, 2);
  {
    // Scratch(arena);
    for (int i = 2; i < 9; ++i) {
      *Push(&fibs, arena) = fibs.data[i - 2] + fibs.data[i - 1];
    }
    ALOG(arena);
  }
  ALOG(arena);

  return fibs;
}

#include "json.h"
void test_json() {

  char json_str[] = "{\"name\":{\"first\":\"Janet\",\"last\":\"Prichard\"},\"age\":47}";

  // Get the last name by its path.
  // A path is a series of keys separated by a dot.
  struct json value = json_get(json_str, "name.last");
  char last_name[64];
  json_string_copy(value, last_name, sizeof(last_name));

  // Get the age as an integer.
  int64_t age = json_int64(json_get(json_str, "age"));

  printf("%s %ld\n", last_name, age);
}

#include "cc.h"
int test_vec() {
  cc_vec(int) our_vec;
  cc_init(&our_vec);

  // Adding elements to end.
  for (int i = 0; i < 10; ++i)
    if (!cc_push(&our_vec, i)) {
      // Out of memory, so abort.
      cc_cleanup(&our_vec);
      return 1;
    }

  // Inserting an element at an index.
  for (int i = 0; i < 10; ++i)
    if (!cc_insert(&our_vec, i * 2, i)) {
      // Out of memory, so abort.
      cc_cleanup(&our_vec);
      return 1;
    }

  // Retrieving and erasing elements.
  for (size_t i = 0; i < cc_size(&our_vec);)
    if (*cc_get(&our_vec, i) % 3 == 0)
      cc_erase(&our_vec, i);
    else
      ++i;

  // Iteration #1.
  cc_for_each(&our_vec, el) printf("%d ", *el);
  // Printed: 1 1 2 2 4 4 5 5 7 7 8 8

  // Iteration #2.
  for (int* el = cc_first(&our_vec); el != cc_end(&our_vec); el = cc_next(&our_vec, el))
    printf("%d ", *el);
  // Printed: Same as above.

  puts("");

  cc_cleanup(&our_vec);

  return 0;
}

struct point {
  double x;
  double y;
};

struct point* test_init(Arena* a, double x, double y) {
  struct point* p = New(a, struct point);
  p->x = x;
  p->y = y;
  return p;
}

#include "adt.h"

UTEST(stc, adt) {
  enum { size = MB(1) };
  byte mem[size] = {0};
  Arena arena[] = {arena_init(mem, size)};
  ASSERT_EQ(15, tree_sum(mkTree(arena)));
}

#include "object.h"

UTEST(interface99, oop) {
  enum { size = KB(1) };
  byte mem[size] = {0};
  Arena arena[] = {arena_init(mem, size)};

  IShape r = newRectangle(arena, 5, 7);
  int p = VCALL(r, perim);
  ASSERT_EQ(p, 24);
  VCALL(r, scale, 5);
  p = VCALL(r, perim);
  ASSERT_EQ(p, 120);

  IShape t = newTriangle(arena, 5, 7, 3);
  p = VCALL(t, perim);
  ASSERT_EQ(p, 15);
  VCALL(t, scale, 5);
  p = VCALL(t, perim);
  ASSERT_EQ(p, 75);
}

int main(int argc, const char* argv[]) {
#ifdef __COSMOCC__
  ShowCrashReports();
#endif

  Arena* arena = arena_default();

  jmp_buf jmpbuf;
  if (ArenaOOM(arena, jmpbuf)) {
    fputs("!!! OOM_DIE !!!\n", stderr);
    exit(1);
  }

  ALOG(arena);
  int test_pqueue(Arena arena);
  test_pqueue(*arena);
  ALOG(arena);

  i64s fibs = test_slice(arena);
  fibs.cap = 0;
  *Push(&fibs, arena) = 0;
  puts(">>>fibs");
  for (int i = 0; i < fibs.len; ++i)
    printf("%ld,", fibs.data[i]);
  puts("<<<fibs");

  astr s = test_astr(arena);
  printf("test_astr: %.*s\n", S(s));

  // char* cs = astr_to_cstr(*arena, s);
  __autofree char* cs = astr_cstrdup(s);
  for (char* p = cs; *p; ++p) {
    New(arena, char, MB(1), OOM_NULL);
    char c = *p;
    if (c >= 'a' && c <= 'z') {
      *p -= 'a' - 'A';
    }
  }
  printf("astr_to_cstr: %s\n", cs);
  ALOG(arena);

  ALOG(arena);
  struct point* p = test_init(arena, 1.0, 2.0);
  struct point* p2 = New(arena, struct point, 1, p);
  ULOG(p2);
  p2->x += 10;
  ULOG(p);
  ALOG(arena);

#ifdef OOM_COMMIT
  arena_os_decommit(arena->beg, arena->end - arena->beg);
#endif

  return utest_main(argc, argv);
}

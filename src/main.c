#if defined(LOGGING) && defined(__linux__)
#define UPRINTF_IMPLEMENTATION
#include "uprintf.h"
#endif

#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>

#define ARENA_COMMIT_PAGE_COUNT 4
#include "arena.h"
#include "debug.h"
#include "utest.h"
UTEST_STATE();

astr test_astr(Arena *arena) {
  ALOG(arena);
  astr s3 = {0};

  {
    Scratch(arena);
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
      New(arena, long);
      printf("'%s'\n", astr_to_cstr(*arena, it.token));
      i++;
    }
    printf("num of token=%d\n", i);

    autofree char *cs = astr_cstrdup(s3);
    for (char *p = cs; *p; ++p) {
      *p += 30;
    }
    printf("test_astr: cs=%s\n", cs);
    ALOG(arena);
  }

  ALOG(arena);
  return astr_clone(arena, s3);
}

typedef Vec(int64_t) i64vec;

i64vec test_slice(Arena *arena) {
  // Int64s fibs = {0};
  // fibs = slice(arena, fibs);
  // *Push(&fibs, arena) = 2;
  // *Push(&fibs, arena) = 3;
  int64_t data[] = {2, 3, 42};
  i64vec fibs = {.data = data, .len = countof(data)};
  fibs = Slice(arena, fibs, 0, 2);
  {
    Scratch(arena);
    for (int i = 2; i < 8; ++i) {
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
  for (int *el = cc_first(&our_vec); el != cc_end(&our_vec); el = cc_next(&our_vec, el))
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

struct point *test_init(Arena a, double x, double y) {
  {
    struct point *p = New(&a, struct point);
    p->x = x;
    p->y = y;
    return p;
  }
}

#include "interface99.h"
#define Shape_IFACE              \
  vfunc(int, perim, const VSelf) \
  vfunc(void, scale, VSelf, int factor)

interface(Shape);

// Rectangle implementation
// ============================================================

typedef struct {
  int a, b;
} Rectangle;

int Rectangle_perim(const VSelf) {
  VSELF(const Rectangle);
  return (self->a + self->b) * 2;
}

void Rectangle_scale(VSelf, int factor) {
  VSELF(Rectangle);
  self->a *= factor;
  self->b *= factor;
}

impl(Shape, Rectangle);

// Triangle implementation
// ============================================================

typedef struct {
  int a, b, c;
} Triangle;

int Triangle_perim(const VSelf) {
  VSELF(const Triangle);
  return self->a + self->b + self->c;
}

void Triangle_scale(VSelf, int factor) {
  VSELF(Triangle);
  self->a *= factor;
  self->b *= factor;
  self->c *= factor;
}

impl(Shape, Triangle);

// Test
// ============================================================

void test_vcall(Shape shape) {
  printf("perim = %d\n", VCALL(shape, perim));
  VCALL(shape, scale, 5);
  printf("perim = %d\n", VCALL(shape, perim));
}

#include "adt.h"

int main(int argc, const char *argv[]) {
#ifdef __COSMOCC__
  ShowCrashReports();
#endif

  enum { ARENA_SIZE = MB(1) };
#ifdef OOM_COMMIT
  Arena arena[] = {arena_init(0, 0)};
#else
  void *mem = malloc(ARENA_SIZE);
  Arena arena[] = {arena_init(mem, ARENA_SIZE)};
#endif

  if (ArenaOOM(arena)) {
    fputs("!!! OOM_DIE !!!\n", stderr);
    exit(1);
  }

  int test_string();
  test_string();
  test_vec();

  ALOG(arena);
  int test_pqueue(Arena * arena);
  test_pqueue(arena);
  ALOG(arena);

  i64vec fibs = test_slice(arena);
  fibs.cap = 0;
  *Push(&fibs, arena) = 0;
  puts(">>>fibs");
  for (int i = 0; i < fibs.len; ++i)
    printf("%ld,", fibs.data[i]);
  puts("<<<fibs");

  int test_vt(Arena * arena);
  ALOG(arena);
  test_vt(arena);
  ALOG(arena);

  astr s = test_astr(arena);
  printf("main: %.*s\n", S(s));

  ALOG(arena);
  struct point *p = test_init(*arena, 1.0, 2.0);
  struct point *p2 = New(arena, struct point, 1, p);
  ULOG(p2);
  ALOG(arena);

  Shape r = DYN_LIT(Rectangle, Shape, {5, 7});
  Shape t = DYN_LIT(Triangle, Shape, {10, 20, 30});

  test_vcall(r);
  test_vcall(t);

  return utest_main(argc, argv);
}

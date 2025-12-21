/**
 * @file arena.h
 * @brief A fast, region-based memory allocator with optional commit-on-demand support.
 *
 * This arena allocator provides a simple, efficient way to manage memory in a contiguous
 * region. It supports both immediate allocation and commit-on-demand via mmap (when
 * OOM_COMMIT is defined). Key features:
 * - Fast allocation with minimal overhead
 * - Optional zero-initialization
 * - slice and string utilities
 * - OOM handling via setjmp/longjmp or NULL return
 *
 * Credit:
 * - https://nullprogram.com/blog/2023/09/27/
 * - https://nullprogram.com/blog/2023/10/05/
 * - https://www.chiark.greenend.org.uk/~sgtatham/quasiblog/c11-generic/#inline
 */

#ifndef ARENA_H_
#define ARENA_H_

#include <memory.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __GNUC__
#define ARENA_INLINE static inline __attribute__((always_inline))
#else
#define ARENA_INLINE static inline
#endif

#ifdef __GNUC__
#define ARENA_LIKELY(xp)   __builtin_expect((bool)(xp), true)
#define ARENA_UNLIKELY(xp) __builtin_expect((bool)(xp), false)
#else
#define ARENA_LIKELY(xp)   (xp)
#define ARENA_UNLIKELY(xp) (xp)
#endif

#ifdef __has_feature
  #if __has_feature(address_sanitizer)
    #define ASAN_ENABLED
  #endif
#elif defined(__SANITIZE_ADDRESS__)
  #define ASAN_ENABLED
#endif

#ifdef ASAN_ENABLED
  #include <sanitizer/asan_interface.h>
#else
  #define ASAN_POISON_MEMORY_REGION(addr, size)   ((void)(addr), (void)(size))
  #define ASAN_UNPOISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#endif

#ifdef __clang__
#define DEBUG_TRAP() __builtin_debugtrap();
#elif defined(__x86_64__)
#define DEBUG_TRAP() __asm__("int3; nop");
#elif defined(__GNUC__)
#define DEBUG_TRAP() __builtin_trap();
#else
#include <signal.h>
#define DEBUG_TRAP() raise(SIGTRAP);
#endif

#ifndef NDEBUG
#define ASSERT_LOG(c) fprintf(stderr, "Assertion failed: " c " at %s %s:%d\n", __func__, __FILE__, __LINE__)
#else
#define ASSERT_LOG(c) (void)0
#endif

#define Assert(c)               \
  do {                          \
    if (ARENA_UNLIKELY(!(c))) { \
      ASSERT_LOG(#c);           \
      DEBUG_TRAP();             \
    }                           \
  } while (0)


#ifdef __GNUC__
static void autofree_impl(void *p) {
  free(*((void **)p));
}
#define __autofree     __attribute__((__cleanup__(autofree_impl)))
#define __arena_reset  __attribute__((__cleanup__(arena_reset_impl)))
#else
#warning "__autofree not supported!"
#define __autofree
#define __arena_reset
#endif

#define Min(a, b) ((a) < (b) ? (a) : (b))
#define Max(a, b) ((a) > (b) ? (a) : (b))

#define KB(n) (((size_t)(n)) << 10)
#define MB(n) (((size_t)(n)) << 20)
#define GB(n) (((size_t)(n)) << 30)
#define TB(n) (((size_t)(n)) << 40)

typedef ptrdiff_t isize;

#ifndef countof
#define countof(arr) ((isize)(sizeof(arr) / sizeof((arr)[0])))
#endif

// - https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66110
// - https://software.codidact.com/posts/280966
typedef unsigned char byte;

typedef struct Arena Arena;
struct Arena {
  byte *beg;
  byte *cur;
  byte *end;
#ifndef OOM_TRAP
  jmp_buf *oom;
#endif
#ifdef OOM_COMMIT
  isize commit_size;
#endif
};

enum {
  _NO_INIT = 1u << 0,   // don't `zero` alloced memory
  _OOM_NULL = 1u << 1,  // return NULL on OOM
};

typedef struct {
  unsigned mask;
} ArenaFlag;

static const ArenaFlag NO_INIT = {_NO_INIT};
static const ArenaFlag OOM_NULL = {_OOM_NULL};

/** Usage:

#define ARENA_SIZE MB(128)

#ifdef OOM_COMMIT
  Arena arena[] = { arena_init(0, 0) };
#else
  autofree void *mem = malloc(ARENA_SIZE);
  Arena arena[] = { arena_init(mem, ARENA_SIZE) };
#endif

  jmp_buf jmpbuf;
  if (ArenaOOM(arena, jmpbuf)) {
    abort();
  }

  {
    Scratch(arena);

    Ty *x = New(arena, Ty);

    // x pointer cannot escape this block

  }

*/

#define New(...)                  _NEWX(__VA_ARGS__, _NEW4, _NEW3, _NEW2)(__VA_ARGS__)
#define _NEWX(a, b, c, d, e, ...) e
#define _NEW2(a, t)               (t *)arena_alloc(a, sizeof(t), _Alignof(t), 1, (ArenaFlag){0})
#define _NEW3(a, t, n)            (t *)arena_alloc(a, sizeof(t), _Alignof(t), n, (ArenaFlag){0})
#define _NEW4(a, t, n, z)                                                   \
  ({                                                                        \
    __auto_type _z = (z);                                                   \
    (t *)_Generic(_z, t *: arena_alloc_init, ArenaFlag: arena_alloc)        \
      (a, sizeof(t), _Alignof(t), n, _Generic(_z, t *: _z, ArenaFlag: _z)); \
  })

#define CONCAT_(a, b) a##b
#define CONCAT(a, b)  CONCAT_(a, b)
#define ARENA_ORIG    CONCAT(_arena_, __LINE__)

#define Scratch(arena)                     \
  __arena_reset Arena ARENA_ORIG = *arena; \
  Arena arena[] = {ARENA_ORIG}

static void arena_reset_impl(Arena *a) {
  // Poison the memory that was used inside the scratch block
  // to catch pointers that "escaped" the block.
  ASAN_POISON_MEMORY_REGION(a->cur, a->end - a->cur);
}

#define slice(T)     \
  struct slice_##T { \
    T *data;         \
    isize len;       \
    isize cap;       \
  }

#define Push(slice, arena)                                                               \
  ({                                                                                     \
    Assert((slice)->len >= 0 && "slice.len must be non-negative");                       \
    Assert((slice)->cap >= 0 && "slice.cap must be non-negative");                       \
    Assert(!((slice)->len == 0 && (slice)->data != NULL) && "Invalid slice");            \
    __auto_type _s = slice;                                                              \
    if (_s->len >= _s->cap) {                                                            \
      arena_slice_grow((arena), _s, sizeof(*_s->data), _Alignof(__typeof__(*_s->data))); \
    }                                                                                    \
    _s->data + _s->len++;                                                                \
  })

#define Clone(...)                   _CloneX(__VA_ARGS__, _Clone4, _Clone3, _Clone2)(__VA_ARGS__)
#define _CloneX(a, b, c, d, e, ...)  e
#define _Clone2(arena, slice)        _Clone3(arena, slice, 0)
#define _Clone3(arena, slice, start) _Clone4(arena, slice, start, slice.len - (start))
#define _Clone4(arena, slice, start, length)                                     \
  ({                                                                             \
    Assert(start >= 0 && "slice start must be non-negative");                    \
    Assert(length >= 0 && "slice length must be non-negative");                  \
    Assert(slice.len >= (start) + (length) && "Invalid slice range");            \
    __auto_type _s = slice;                                                      \
    _s.cap = _s.len = length;                                                    \
    if (_s.len == 0) {                                                           \
      _s.data = NULL;                                                            \
    }                                                                            \
    if (_s.data) {                                                               \
      _s.data = New(arena, __typeof__(_s.data[0]), _s.len, (_s.data + (start))); \
    }                                                                            \
    _s;                                                                          \
  })


#ifdef OOM_COMMIT

#include <sys/mman.h>
#include <unistd.h>
static size_t arena_os_pagesize(void) {
  return (size_t)sysconf(_SC_PAGESIZE);
}
static void* arena_os_reserve(size_t size) {
  void* ptr = mmap(0, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  return (ptr == MAP_FAILED) ? NULL : ptr;
}
static bool arena_os_commit(void* ptr, size_t size) {
  return mprotect(ptr, size, PROT_READ | PROT_WRITE) == 0;
}
static void arena_os_decommit(void* ptr, size_t size) {
  madvise(ptr, size, POSIX_MADV_DONTNEED);
}

#ifndef ARENA_COMMIT_PAGE_COUNT
#define ARENA_COMMIT_PAGE_COUNT 1024
#endif
#define ARENA_RESERVE_PAGE_COUNT (1024 * ARENA_COMMIT_PAGE_COUNT)

#endif

ARENA_INLINE Arena arena_init(byte *buf, isize size) {
  Arena a = {0};
#ifdef OOM_COMMIT
  isize page_size = arena_os_pagesize();
  a.commit_size = page_size * ARENA_COMMIT_PAGE_COUNT;
  if (size == 0) {
    size = a.commit_size;
    buf = arena_os_reserve(page_size * ARENA_RESERVE_PAGE_COUNT);
    if (!buf) {
      perror("arena_init mmap");
      Assert(!"arena_init mmap");
    }
    Assert(arena_os_commit(buf, a.commit_size));
  }
#endif
  a.beg = a.cur = buf;
  a.end = buf ? buf + size : 0;

  ASAN_POISON_MEMORY_REGION(buf, size);
  return a;
}

ARENA_INLINE void arena_reset(Arena *arena) {
  ASAN_POISON_MEMORY_REGION(arena->beg, arena->end - arena->beg);
  arena->cur = arena->beg;
}

// Safe multiplication using compiler intrinsics where available
ARENA_INLINE bool arena_mul_overflow(size_t a, size_t b, isize *res) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_mul_overflow(a, b, res);
#else
    *res = a * b;
    return (a != 0 && *res / a != b);
#endif
}

#ifndef OOM_TRAP
#define ArenaOOM(arena, jmpbuf)   ((arena)->oom = &jmpbuf, setjmp(jmpbuf))
#else
#define ArenaOOM(arena, jmpbuf)   ((void)jmpbuf, false)
#endif

static void *arena_alloc_grow(Arena *arena, isize size, isize align, isize count, ArenaFlag flags) {
  byte *current = arena->cur;
  isize avail = arena->end - current;
  isize pad = -(uintptr_t)current & (align - 1);

  isize total_size;
  if (ARENA_UNLIKELY(arena_mul_overflow(size, count, &total_size))) {
      goto HANDLE_OOM;
  }

  while (count >= (avail - pad) / size) {
#ifdef OOM_COMMIT
    // arena->commit_size == 0 if arena was malloced
    if (arena->commit_size) {
      if (!arena_os_commit(arena->end, arena->commit_size)) {
        perror("arena_alloc mprotect");
        goto HANDLE_OOM;
      }
      arena->end += arena->commit_size;
      avail = arena->end - current;

      ASAN_POISON_MEMORY_REGION(arena->end, arena->commit_size);
      continue;
    }
#endif
    goto HANDLE_OOM;
  }

  arena->cur += pad + total_size;
  current += pad;

  ASAN_UNPOISON_MEMORY_REGION(current, total_size);
  return flags.mask & _NO_INIT ? current : memset(current, 0, total_size);

HANDLE_OOM:
  if (flags.mask & _OOM_NULL)
    return NULL;
#ifdef OOM_TRAP
  Assert(!OOM_TRAP);
#else
  Assert(arena->oom);
  longjmp(*arena->oom, 1);
#endif
  return NULL;
}

ARENA_INLINE void *arena_alloc(Arena *arena, isize size, isize align, isize count, ArenaFlag flags) {
  byte *current = arena->cur;
  isize pad = -(uintptr_t)current & (align - 1);
  isize avail = arena->end - current;
  if (ARENA_LIKELY(count <= (avail - pad) / size)) {
    isize total_size = size * count;
    arena->cur += pad + total_size;
    current += pad;
    ASAN_UNPOISON_MEMORY_REGION(current, total_size);
    return flags.mask & _NO_INIT ? current : memset(current, 0, total_size);
  }

  return arena_alloc_grow(arena, size, align, count, flags);
}

ARENA_INLINE void *arena_alloc_init(Arena *arena, isize size, isize align, isize count,
                                    const void *initptr) {
  Assert(initptr != NULL && "initptr cannot be NULL");
  void *ptr = arena_alloc(arena, size, align, count, NO_INIT);
  memmove(ptr, initptr, size * count);
  return ptr;
}

ARENA_INLINE void arena_slice_grow(Arena *arena, void *slice, isize size, isize align) {
  struct {
    void *data;
    isize len;
    isize cap;
  } tmp;
  memcpy(&tmp, slice, sizeof(tmp));

  isize grow = 10 * size;

  if (tmp.cap == 0) {
    // move slice from stack
    tmp.cap = tmp.len + grow;
    void *ptr = arena_alloc(arena, size, align, tmp.cap, NO_INIT);
    tmp.data = tmp.len == 0 ? ptr : memmove(ptr, tmp.data, size * tmp.len);
  } else if (ARENA_LIKELY((uintptr_t)tmp.data == (uintptr_t)arena->cur - size * tmp.cap)) {
    // grow slice inplace
    tmp.cap += grow;
    arena_alloc(arena, size, 1, grow, NO_INIT);
  } else {
    tmp.cap += Max(tmp.cap / 2, grow);
    void *ptr = arena_alloc(arena, size, align, tmp.cap, NO_INIT);
    tmp.data = memmove(ptr, tmp.data, size * tmp.len);
  }

  memcpy(slice, &tmp, sizeof(tmp));
}

ARENA_INLINE void *arena_malloc(size_t size, Arena *arena) {
  Assert(arena != NULL && "arena cannot be NULL");
  return arena_alloc(arena, size, _Alignof(max_align_t), 1, NO_INIT);
}

ARENA_INLINE void arena_free(void *ptr, size_t size, Arena *arena) {
  Assert(arena != NULL && "arena cannot be NULL");
  if (!ptr) return;

  if ((uintptr_t)ptr == (uintptr_t)arena->cur - size) {
    ASAN_POISON_MEMORY_REGION(ptr, size);
    arena->cur = ptr;
  }
}

// Arena owned str (aka astr)
typedef struct astr {
  char *data;
  isize len;
} astr;

// string literal only!
#define astr(s) ((astr){(s), sizeof(s) - 1})

// printf("%.*s", S(s))
#define S(s) (int)(s).len, (s).data

ARENA_INLINE astr astr_clone(Arena *arena, astr s) {
  // Early return if string is empty or already at arena tip
  if (s.len == 0 || s.data + s.len == (char *)arena->cur)
    return s;

  astr s2 = s;
  s2.data = New(arena, char, s.len, NO_INIT);
  memmove(s2.data, s.data, s.len);
  return s2;
}

ARENA_INLINE astr astr_concat(Arena *arena, astr head, astr tail) {
  // Ignore empty head
  if (head.len == 0) {
    // If tail is at arena tip, return it directly; otherwise clone
    return tail.len && tail.data + tail.len == (char *)arena->cur ? tail : astr_clone(arena, tail);
  }

  astr result = head;
  result = astr_clone(arena, head);
  // result is guaranteed to be at arena tip, clone tail and append it
  result.len += astr_clone(arena, tail).len;
  return result;
}

ARENA_INLINE astr astr_from_bytes(Arena *arena, const void *bytes, size_t nbytes) {
  return astr_clone(arena, (astr){(char *)bytes, nbytes});
}

ARENA_INLINE astr astr_from_cstr(Arena *arena, const char *str) {
  return astr_from_bytes(arena, str, strlen(str));
}

ARENA_INLINE astr astr_cat_bytes(Arena *arena, astr head, const void *bytes, size_t nbytes) {
  return astr_concat(arena, head, (astr){(char *)bytes, nbytes});
}

ARENA_INLINE astr astr_cat_cstr(Arena *arena, astr head, const char *str) {
  return astr_cat_bytes(arena, head, str, strlen(str));
}

static astr astr_format(Arena *arena, const char *format, ...) {
  va_list args;
  va_start(args, format);
  int nbytes = vsnprintf(NULL, 0, format, args);
  va_end(args);
  Assert(nbytes >= 0);

  void *data = New(arena, char, nbytes + 1, NO_INIT);
  va_start(args, format);
  int nbytes2 = vsnprintf(data, nbytes + 1, format, args);
  va_end(args);
  Assert(nbytes2 == nbytes);

  // drop \0 so that astr_concat still works
  arena->cur--;

  return (astr){.data = data, .len = nbytes};
}

// return a null-terminated byte string with a temporary lifetime.
ARENA_INLINE const char *astr_to_cstr(Arena arena, astr s) {
  return astr_concat(&arena, s, astr("\0")).data;
}

// just like strndup, caller must free it!
ARENA_INLINE char *astr_cstrdup(astr s) {
  return strndup(s.data, s.len);
}

ARENA_INLINE astr _astr_split_by_char(astr s, const char *charset, isize *pos, Arena *a) {
  astr slice = {s.data + *pos, s.len - *pos};
  const char *p1 = astr_to_cstr(*a, slice);
  const char *p2 = strpbrk(p1, charset);
  astr token = {slice.data, p2 ? (p2 - p1) : slice.len};
  isize sep_len = p2 ? strspn(p2, charset) : 0;  // skip seperator found in charset
  *pos += token.len + sep_len;
  return token;
}

// for (astr_split_by_char(it, ",| ", str, arena)) { ... it.token ...}
#define astr_split_by_char(it, charset, str, arena) \
  struct {                                          \
    astr input, token;                              \
    const char *sep;                                \
    isize pos;                                      \
  } it = {.input = str, .sep = charset};            \
  it.pos < it.input.len && (it.token = _astr_split_by_char(it.input, it.sep, &it.pos, arena)).data[0];

ARENA_INLINE astr _astr_split(astr s, astr sep, isize *pos) {
  astr slice = {s.data + *pos, s.len - *pos};
  const char *res = memmem(slice.data, slice.len, sep.data, sep.len);
  astr token = {slice.data, res && res != slice.data ? (res - slice.data) : slice.len};
  *pos += token.len + sep.len;
  return token;
}

// for (astr_split(it, ", ", str)) { ... it.token ...}
#define astr_split(it, strsep, str)                             \
  struct {                                                      \
    astr input, token, sep;                                     \
    isize pos;                                                  \
  } it = {.input = str, .sep = (astr){strsep, strlen(strsep)}}; \
  it.pos < it.input.len && (it.token = _astr_split(it.input, it.sep, &it.pos)).data;

ARENA_INLINE bool astr_equals(astr a, astr b) {
  if (a.len != b.len)
    return false;

  return !a.len || !memcmp(a.data, b.data, a.len);
}

ARENA_INLINE bool astr_starts_with(astr s, astr prefix) {
  isize n = prefix.len;
  return n <= s.len && !memcmp(s.data, prefix.data, n);
}

ARENA_INLINE bool astr_ends_with(astr s, astr suffix) {
  isize n = suffix.len;
  return n <= s.len && !memcmp(s.data + s.len - n, suffix.data, n);
}

ARENA_INLINE astr astr_substr(astr s, isize pos, isize len) {
    Assert(((size_t)pos <= (size_t)s.len) & (len >= 0));
    if (pos + len > s.len) len = s.len - pos;
    s.data += pos, s.len = len;
    return s;
}

ARENA_INLINE astr astr_slice(astr s, isize p1, isize p2) {
    Assert(((size_t)p1 <= (size_t)p2) & ((size_t)p1 <= (size_t)s.len));
    if (p2 > s.len) p2 = s.len;
    s.data += p1, s.len = p2 - p1;
    return s;
}

ARENA_INLINE astr astr_trim_left(astr s) {
  while (s.len && *s.data <= ' ')
    ++s.data, --s.len;
  return s;
}

ARENA_INLINE astr astr_trim_right(astr s) {
  while (s.len && s.data[s.len - 1] <= ' ')
    --s.len;
  return s;
}

ARENA_INLINE astr astr_trim(astr sv) {
  return astr_trim_right(astr_trim_left(sv));
}

ARENA_INLINE uint64_t astr_hash(astr key) {
  uint64_t hash = 0xcbf29ce484222325ull;
  for (isize i = 0; i < key.len; i++)
    hash = ((byte)key.data[i] ^ hash) * 0x100000001b3ull;

  return hash;
}

/** Usage:

#include "cc.h"

static inline uint64_t astr_wyhash(astr key) {
  return cc_wyhash(key.data, key.len);
}

static inline void *vt_arena_malloc(size_t size, Arena **ctx) {
  return arena_malloc(size, *ctx);
}

static inline void vt_arena_free(void *ptr, size_t size, Arena **ctx) {
  arena_free(ptr, size, *ctx);
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

*/

#endif  // ARENA_H_

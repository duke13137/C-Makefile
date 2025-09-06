#include <stc/coroutine.h>
#include <stdio.h>
#include "utest.h"

struct Gen {
  cco_base base;
  int start, end, value;
};

static int Gen(struct Gen *g) {
  cco_async(g) {
    for (g->value = g->start; g->value < g->end; ++g->value)
      cco_yield;
  }
  return 0;
}

UTEST(stc, coroutine) {
  struct Gen gen = {.start = 10, .end = 20};

  cco_run_coroutine(Gen(&gen)) {
    printf("%d, ", gen.value);
  }

  puts("");
}

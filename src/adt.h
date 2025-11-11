// clang-format off
#pragma once

#include "stc/algorithm.h"

#include "arena.h"
#include "stc/sys/sumtype.h"
#include "utest.h"

#ifndef _CLANGD
#include "datatype99.h"

#define EXPR(expr)       New(arena, Expr, 1, (Expr[]){expr})
#define OP(lhs, op, rhs) EXPR(op(EXPR(lhs), EXPR(rhs)))

datatype(
    Expr,
    (Const, double),
    (Add, Expr *, Expr *),
    (Sub, Expr *, Expr *),
    (Mul, Expr *, Expr *),
    (Div, Expr *, Expr *)
);

static double eval(const Expr *expr) {
  match(*expr) {
    of(Const, number) return *number;
    of(Add, lhs, rhs) return eval(*lhs) + eval(*rhs);
    of(Sub, lhs, rhs) return eval(*lhs) - eval(*rhs);
    of(Mul, lhs, rhs) return eval(*lhs) * eval(*rhs);
    of(Div, lhs, rhs) return eval(*lhs) / eval(*rhs);
  }

  Assert("Invalid expr");
  return -1;
}

static Expr *expr(Arena *arena) {
  return OP(*OP(*OP(Const(53), Add, Const(5)), Sub, Const(10)), Div, Const(8));
}

UTEST(datatype99, adt) {
  enum { size = KB(1) };
  byte mem[size] = {0};
  Arena arena = arena_init(mem, size);
  ASSERT_EQ(6.0, eval(expr(&arena)));
}

#endif // _CLANGD

c_union(Tree,
  (Empty, bool),
  (Leaf, int),
  (Node, struct { int value; Tree *left, *right; }));

static int tree_sum(Tree *t) {
  c_when(t) {
    c_is(Empty) return 0;
    c_is(Leaf, v) return *v;
    c_is(Node, n) return n->value + tree_sum(n->left) + tree_sum(n->right);
  }
  return -1;
}

#define newTree(tag, ...)  New(arena, Tree, 1, &c_variant(tag, __VA_ARGS__))

static Tree* mkTree(Arena *arena) {
  Tree* tree =
    newTree(Node, {1,
                   newTree(Node, {2,
                                  newTree(Leaf, 3),
                                  newTree(Leaf, 4)
                   }),
                   newTree(Leaf, 5)});

    return tree;
}


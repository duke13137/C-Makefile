// clang-format off
#pragma once

#include "interface99.h"
#include "stc/algorithm.h"

#include "arena.h"
#include "utest.h"

#ifndef _CLANGD
#include "datatype99.h"

#define EXPR(expr)       New(arena, Expr, 1, ((Expr[]){expr}))
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
  byte mem[KB(1)] = {0};
  Arena arena = arena_init(mem, KB(1));
  ASSERT_EQ(6.0, eval(expr(&arena)));
}

#endif

#define Shape_IFACE                      \
    vfunc( int, perim, const VSelf)      \
    vfunc(void, scale, VSelf, int factor)

interface(Shape);

#define TREE(tree)                New(arena, Tree, 1, &c_variant(Node, tree))
#define NODE(left, number, right) TREE(Node(left, number, right))
#define LEAF(number)              TREE(Leaf(number))

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

UTEST(std, adt) {
  Tree* tree =
  &c_variant(Node, {1,
      &c_variant(Node, {2,
          &c_variant(Leaf, 3),
          &c_variant(Leaf, 4)
      }),
      &c_variant(Leaf, 5)
  });

  ASSERT_EQ(15, tree_sum(tree));
}

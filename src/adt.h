// clang-format off
#pragma once

#include "arena.h"
#include "datatype99.h"
#include "interface99.h"

#define TREE(tree)                New(arena, BinaryTree, 1, (BinaryTree[]){tree})
#define NODE(left, number, right) TREE(Node(left, number, right))
#define LEAF(number)              TREE(Leaf(number))

datatype(
    BinaryTree,
    (Leaf, int),
    (Node, const BinaryTree *, int, const BinaryTree *)
);

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


// clang-format off
#define Shape_IFACE                      \
    vfunc( int, perim, const VSelf)      \
    vfunc(void, scale, VSelf, int factor)
// clang-format on

interface(Shape);

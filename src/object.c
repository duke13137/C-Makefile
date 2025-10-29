#include "object.h"
#include "arena.h"

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

impl(IShape, Rectangle);

IShape newRectangle(Arena* arena, int x, int y) {
  Rectangle* r = New(arena, Rectangle);
  r->a = x;
  r->b = y;
  return DYN(Rectangle, IShape, r);
}

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

impl(IShape, Triangle);

IShape newTriangle(Arena* arena, int x, int y, int z) {
  Triangle* r = New(arena, Triangle);
  r->a = x;
  r->b = y;
  r->c = z;
  return DYN(Triangle, IShape, r);
}

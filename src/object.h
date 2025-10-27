#pragma once

#include "interface99.h"

#define IShape_IFACE             \
  vfunc(int, perim, const VSelf) \
  vfunc(void, scale, VSelf, int factor)

interface(IShape);

typedef struct Arena Arena;

IShape newRectangle(Arena* arena, int x, int y);

IShape newTriangle(Arena* arena, int x, int y, int z);

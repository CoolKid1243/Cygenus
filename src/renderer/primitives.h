#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include "rhi.h"

RHIMesh* primitive_create_cube(void);
RHIMesh* primitive_create_plane(float width, float depth, int resolution);
RHIMesh* primitive_create_sphere(float radius, int segments, int rings);

#endif // PRIMITIVES_H
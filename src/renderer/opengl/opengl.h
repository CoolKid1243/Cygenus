#ifndef OPENGL_H
#define OPENGL_H

#include "../rhi.h"

// Fills in an RHIFunctionTable with OpenGL implementations.
void opengl_register(RHIFunctionTable* table);

#endif
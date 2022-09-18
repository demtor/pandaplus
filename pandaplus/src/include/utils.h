#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

#include "pandos_types.h"

void* memcpy(void* dest, const void* src, size_t n);
unsigned int msb(unsigned int v);
unsigned int lsb(unsigned int v);

#endif

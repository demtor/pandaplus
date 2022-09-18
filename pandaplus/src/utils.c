#include "utils.h"
#include "pandos_types.h"

/*
 * Naive implementation of memcpy
 */
void* memcpy(void* dest, const void* src, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        ((unsigned char*) dest)[i] = ((unsigned char*) src)[i];
    }

    return dest;
}

/*
 * Most significant bit (naive)
 * Returns the position of the most significant set bit (1 bit)
 * if v is 0, the return is undefined
 */
unsigned int msb(unsigned int v)
{
    unsigned int pos = 0;
    while (v >>= 1) {
        ++pos;
    }

    return pos;
}

/*
 * Less significant bit (naive)
 * Returns the position of the less significant set bit (1 bit)
 * if v is 0, the return is undefined
 */
unsigned int lsb(unsigned int v)
{
    unsigned int pos = 0;

    if (v != 0) {
        while (!(v & 0x1)) {
            v >>= 1;
            ++pos;
        }
    }

    return pos;
}

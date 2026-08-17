#ifndef LIB_STRING_H
#define LIB_STRING_H

#include <stddef.h>

/* Freestanding subset actually used by the kernel, not a libc replacement.
 * memcpy/memset/memmove/memcmp also double as compiler builtins: Clang
 * lowers struct assignment, array init, and some loops to calls to these
 * exact symbols even in freestanding mode, so they must exist with
 * standard signatures regardless of whether kernel code calls them
 * directly. */

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *dest, int value, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *a, const void *b, size_t n);

size_t strlen(const char *str);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);

#endif

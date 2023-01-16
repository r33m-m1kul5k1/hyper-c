#pragma once
#include <stddef.h>

void strcpy(char *dst, const char *src);
int strlen(const char *str);
void* memcpy( void *dest, const void *src, size_t count );
const char *get_number_start(const char *str);
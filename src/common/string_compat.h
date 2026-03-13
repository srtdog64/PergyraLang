#ifndef PERGYRA_STRING_COMPAT_H
#define PERGYRA_STRING_COMPAT_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static inline char *
pergyra_strndup(const char *src, size_t length)
{
    char *copy = malloc(length + 1);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, src, length);
    copy[length] = '\0';
    return copy;
}

static inline char *
pergyra_strdup(const char *src)
{
    return pergyra_strndup(src, strlen(src));
}

#endif

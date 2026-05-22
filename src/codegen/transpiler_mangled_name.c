/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend type/name mangling helpers.
 */

#include "transpiler_mangled_name.h"

void
append_mangled_type_name(CodeBuf *buf, const char *type_name)
{
    bool wrote = false;

    if (buf == NULL || type_name == NULL) {
        if (buf != NULL)
            codebuf_write(buf, "Type");
        return;
    }

    for (const unsigned char *p = (const unsigned char *)type_name; *p != '\0'; p++) {
        if ((*p >= 'a' && *p <= 'z')
            || (*p >= 'A' && *p <= 'Z')
            || (*p >= '0' && *p <= '9')) {
            codebuf_write(buf, "%c", *p);
            wrote = true;
        } else if (wrote && buf->len > 0 && buf->data[buf->len - 1] != '_') {
            codebuf_write(buf, "_");
        }
    }

    if (!wrote)
        codebuf_write(buf, "Type");
}

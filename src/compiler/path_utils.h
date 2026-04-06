/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Path utilities — platform-independent file path operations.
 */

#ifndef PGY_PATH_UTILS_H
#define PGY_PATH_UTILS_H

#include <stdbool.h>

/* Return the directory part of path (heap-allocated). "a/b.c" → "a" */
char *path_dirname_dup(const char *path);

/* Join directory and filename (heap-allocated). ("a", "b.c") → "a/b.c" */
char *path_join_dup(const char *dir, const char *path);

/* Replace file extension (heap-allocated). ("a.pgy", ".c") → "a.c" */
char *path_replace_extension(const char *path, const char *new_ext);

/* Read entire file into heap-allocated string. Returns NULL on failure. */
char *path_read_file(const char *path);

/* Default binary output path for a source file. */
char *path_default_binary(const char *source_path);

/* True if the given path exists as a regular filesystem entry. */
bool path_file_exists(const char *path);

/*
 * Resolve a runnable binary path for the host platform.
 * On Windows toolchains, this accepts callers passing "foo" and returns
 * "foo.exe" if that is the actual output emitted by the linker.
 */
char *path_resolve_runnable_binary(const char *path);

#endif /* PGY_PATH_UTILS_H */

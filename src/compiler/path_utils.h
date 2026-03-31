/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Path utilities — platform-independent file path operations.
 */

#ifndef PGY_PATH_UTILS_H
#define PGY_PATH_UTILS_H

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

#endif /* PGY_PATH_UTILS_H */

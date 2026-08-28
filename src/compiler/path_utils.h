/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Path utilities — platform-independent file path operations.
 */

#ifndef PGY_PATH_UTILS_H
#define PGY_PATH_UTILS_H

#include <stddef.h>
#include <stdbool.h>

#ifndef PGY_MAX_TEXT_FILE_BYTES
#define PGY_MAX_TEXT_FILE_BYTES (64u * 1024u * 1024u)
#endif

/* Return the directory part of path (heap-allocated). "a/b.c" → "a" */
char *path_dirname_dup(const char *path);

/* Join directory and filename (heap-allocated). ("a", "b.c") → "a/b.c" */
char *path_join_dup(const char *dir, const char *path);

/* Replace file extension (heap-allocated). ("a.pgy", ".c") → "a.c" */
char *path_replace_extension(const char *path, const char *new_ext);

/* Read entire file into heap-allocated string. Returns NULL on failure. */
char *path_read_file(const char *path);

typedef enum
{
    PATH_REPLACE_ERROR = 0,
    PATH_REPLACE_OK,
    PATH_REPLACE_SOURCE_CHANGED,
    PATH_REPLACE_RECOVERY_REQUIRED
} PathReplaceFileResult;

/*
 * Reject a destination that already differs from expected_content before any
 * exchange. If a race is observed only after the exchange, rollback is
 * attempted and PATH_REPLACE_RECOVERY_REQUIRED is returned; the caller must
 * preserve both workspace paths because either may contain a concurrent edit.
 * backup_path is a private sibling used by the Windows exchange primitive.
 */
PathReplaceFileResult path_replace_file_atomic_if_unchanged(
    const char *tmp_path,
    const char *dst_path,
    const char *backup_path,
    const char *expected_content);

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

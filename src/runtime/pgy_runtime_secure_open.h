#ifndef PGY_RUNTIME_SECURE_OPEN_H
#define PGY_RUNTIME_SECURE_OPEN_H

/*
 * Secure file open for the sandbox I/O boundary (redteam residual, 2026-07-05).
 *
 * pgy_runtime_resolve_file_path() validates the target BEFORE the open: it
 * realpath-prefix-matches the parent against PGY_IO_ROOT and, on a write,
 * lstat-rejects a symlinked final component. But that check and the open()
 * below are two syscalls with a gap between them -- the classic check-to-use
 * (TOCTOU) window in which an attacker who controls the sandbox tree can swap
 * the final component for a symlink pointing OUTSIDE the sandbox after the
 * check passes but before the write lands.
 *
 * For writes we therefore do not fopen(): we open() the resolved path with
 * O_NOFOLLOW, so the kernel itself refuses (ELOOP) if the final component is a
 * symlink AT OPEN TIME -- atomically, with no window. Reads keep plain fopen():
 * their resolve path realpath()s the FULL candidate and prefix-matches, so a
 * symlink pointing out of the sandbox already fails resolution before any open.
 *
 * Windows keeps fopen(): the resolve path already rejects any reparse-point
 * component (pgy_runtime_path_has_reparse_component). A fully airtight
 * open-time guard there needs CreateFileA + FILE_FLAG_OPEN_REPARSE_POINT and
 * remains a documented residual, not closed here.
 *
 * This is defense-in-depth layered on top of the resolve check, not a
 * replacement for it: both stay load-bearing.
 */

#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

static FILE *
pgy_runtime_secure_fopen(const char *resolved, const char *mode)
{
#ifdef _WIN32
    return fopen(resolved, mode);
#else
    int flags = 0;
    int has_plus;
    int fd;
    char base;
    FILE *fp;

    if (resolved == NULL || mode == NULL || mode[0] == '\0')
        return NULL;

    base = mode[0];
    has_plus = (strchr(mode, '+') != NULL);

    /* Read-only opens: resolve realpath+prefix-rejects a symlink pointing out
     * of the sandbox, so plain fopen carries no escape risk. */
    if (base == 'r' && !has_plus)
        return fopen(resolved, mode);

    if (base == 'w')
        flags = has_plus ? (O_RDWR | O_CREAT | O_TRUNC)
                         : (O_WRONLY | O_CREAT | O_TRUNC);
    else if (base == 'a')
        flags = has_plus ? (O_RDWR | O_CREAT | O_APPEND)
                         : (O_WRONLY | O_CREAT | O_APPEND);
    else if (base == 'r')            /* "r+": read/write existing, no create */
        flags = O_RDWR;
    else
        return fopen(resolved, mode); /* unknown mode: preserve libc behavior */

    flags |= O_NOFOLLOW;
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif

    fd = open(resolved, flags, 0666);
    if (fd < 0)
        return NULL;                  /* ELOOP on a symlinked target lands here */
    fp = fdopen(fd, mode);
    if (fp == NULL)
        close(fd);
    return fp;
#endif
}

#endif /* PGY_RUNTIME_SECURE_OPEN_H */

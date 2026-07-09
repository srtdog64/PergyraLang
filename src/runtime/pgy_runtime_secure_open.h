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
 * For writes we therefore do not fopen(): where the platform exposes
 * O_NOFOLLOW, we open() the resolved path with it so the kernel itself refuses
 * (ELOOP) if the final component is a symlink AT OPEN TIME -- atomically, with
 * no window. POSIX-family platforms without O_NOFOLLOW keep the resolve/lstat
 * guard as a portability fallback. Reads keep plain fopen():
 * their resolve path realpath()s the FULL candidate and prefix-matches, so a
 * symlink pointing out of the sandbox already fails resolution before any open.
 *
 * Windows uses CreateFileA + FILE_FLAG_OPEN_REPARSE_POINT for write-capable
 * opens, then refuses a final component whose handle is a reparse point before
 * converting the handle to a CRT FILE*. This keeps the final-component check
 * tied to the object actually opened, instead of trusting a previous path walk.
 *
 * This is defense-in-depth layered on top of the resolve check, not a
 * replacement for it: both stay load-bearing.
 */

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <stdint.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>

/* Strict C compile paths may not define POSIX feature macros before this
 * generated-runtime header is included, so glibc can hide fdopen(). The
 * secure-open owner declares the POSIX boundary it consumes locally. */
extern FILE *fdopen(int fd, const char *mode);
#endif

#ifdef _WIN32
static FILE *
pgy_runtime_windows_secure_fopen(const char *resolved, const char *mode)
{
    DWORD access = 0;
    DWORD disposition = OPEN_EXISTING;
    DWORD flags = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT;
    int crt_flags = 0;
    int has_plus;
    char base;
    HANDLE handle;
    BY_HANDLE_FILE_INFORMATION info;
    int fd;
    FILE *fp;

    if (resolved == NULL || mode == NULL || mode[0] == '\0')
        return NULL;

    base = mode[0];
    has_plus = (strchr(mode, '+') != NULL);

    if (base == 'r' && !has_plus)
        return fopen(resolved, mode);

    if (base == 'w') {
        access = has_plus ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_WRITE;
        disposition = CREATE_ALWAYS;
        crt_flags = has_plus ? _O_RDWR : _O_WRONLY;
    } else if (base == 'a') {
        access = has_plus ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_WRITE;
        disposition = OPEN_ALWAYS;
        crt_flags = (has_plus ? _O_RDWR : _O_WRONLY) | _O_APPEND;
    } else if (base == 'r') {
        access = GENERIC_READ | GENERIC_WRITE;
        disposition = OPEN_EXISTING;
        crt_flags = _O_RDWR;
    } else {
        return fopen(resolved, mode);
    }

    crt_flags |= (strchr(mode, 'b') != NULL) ? _O_BINARY : _O_TEXT;

    handle = CreateFileA(resolved,
                         access,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         NULL,
                         disposition,
                         flags,
                         NULL);
    if (handle == INVALID_HANDLE_VALUE)
        return NULL;

    if (!GetFileInformationByHandle(handle, &info) ||
        (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        CloseHandle(handle);
        return NULL;
    }

    fd = _open_osfhandle((intptr_t)handle, crt_flags);
    if (fd < 0) {
        CloseHandle(handle);
        return NULL;
    }

    fp = _fdopen(fd, mode);
    if (fp == NULL)
        _close(fd);
    return fp;
}
#endif

static FILE *
pgy_runtime_secure_fopen(const char *resolved, const char *mode)
{
#ifdef _WIN32
    return pgy_runtime_windows_secure_fopen(resolved, mode);
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

#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
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

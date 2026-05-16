#include "compiler_process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

int
pgy_exec_argv(const char *const argv[], bool verbose)
{
    if (verbose) {
        printf("pgy:");
        for (const char *const *p = argv; *p; p++)
            printf(" %s", *p);
        printf("\n");
    }

#ifdef _WIN32
    intptr_t rc = _spawnvp(_P_WAIT, argv[0], argv);
    return (int)rc;
#else
    pid_t pid = fork();
    if (pid < 0)
        return -1;
    if (pid == 0) {
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
#endif
}

#ifdef _WIN32
void
pgy_win32_normalize_exec_path(const char *path, char *dst, size_t dst_cap)
{
    const char *tmpbase = NULL;
    size_t pos = 0;

    if (dst == NULL || dst_cap == 0)
        return;
    dst[0] = '\0';
    if (path == NULL || path[0] == '\0')
        return;

    if (path[0] == '/' && path[1] != '\0' && path[2] == '/'
        && ((path[1] >= 'a' && path[1] <= 'z')
            || (path[1] >= 'A' && path[1] <= 'Z'))) {
        if (dst_cap < 4)
            return;
        dst[pos++] = path[1];
        dst[pos++] = ':';
        dst[pos++] = '\\';
        path += 3;
    } else if (strncmp(path, "/tmp/", 5) == 0 || strcmp(path, "/tmp") == 0) {
        tmpbase = getenv("TMPDIR");
        if (tmpbase == NULL || tmpbase[0] == '\0')
            tmpbase = getenv("TMP");
        if (tmpbase == NULL || tmpbase[0] == '\0')
            tmpbase = getenv("TEMP");
        if (tmpbase != NULL && tmpbase[0] != '\0') {
            size_t base_len = strlen(tmpbase);
            if (base_len >= dst_cap)
                base_len = dst_cap - 1;
            memcpy(dst, tmpbase, base_len);
            pos = base_len;
            if (pos > 0 && dst[pos - 1] != '\\' && dst[pos - 1] != '/') {
                if (pos + 1 >= dst_cap) {
                    dst[dst_cap - 1] = '\0';
                    return;
                }
                dst[pos++] = '\\';
            }
            path += (path[4] == '\0') ? 4 : 5;
        }
    }

    while (*path != '\0' && pos + 1 < dst_cap) {
        dst[pos++] = (*path == '/') ? '\\' : *path;
        path++;
    }
    dst[pos] = '\0';
}

static bool
pgy_win32_quote_arg(char *dst, size_t dst_cap, size_t *pos_io, const char *arg)
{
    size_t pos = pos_io != NULL ? *pos_io : 0;
    bool needs_quotes = false;
    const char *p;

    if (dst == NULL || dst_cap == 0 || pos_io == NULL || arg == NULL)
        return false;

    for (p = arg; *p != '\0'; p++) {
        if (*p == ' ' || *p == '\t' || *p == '"') {
            needs_quotes = true;
            break;
        }
    }

    if (!needs_quotes) {
        size_t n = strlen(arg);
        if (pos + n + 1 >= dst_cap)
            return false;
        memcpy(dst + pos, arg, n);
        pos += n;
        dst[pos] = '\0';
        *pos_io = pos;
        return true;
    }

    if (pos + 2 >= dst_cap)
        return false;
    dst[pos++] = '"';

    p = arg;
    while (*p != '\0') {
        size_t slash_count = 0;
        while (*p == '\\') {
            slash_count++;
            p++;
        }
        if (*p == '"') {
            while (slash_count-- > 0) {
                if (pos + 2 >= dst_cap)
                    return false;
                dst[pos++] = '\\';
                dst[pos++] = '\\';
            }
            if (pos + 2 >= dst_cap)
                return false;
            dst[pos++] = '\\';
            dst[pos++] = '"';
            p++;
            continue;
        }
        if (*p == '\0') {
            while (slash_count-- > 0) {
                if (pos + 2 >= dst_cap)
                    return false;
                dst[pos++] = '\\';
                dst[pos++] = '\\';
            }
            break;
        }
        while (slash_count-- > 0) {
            if (pos + 1 >= dst_cap)
                return false;
            dst[pos++] = '\\';
        }
        if (pos + 1 >= dst_cap)
            return false;
        dst[pos++] = *p++;
    }

    if (pos + 2 >= dst_cap)
        return false;
    dst[pos++] = '"';
    dst[pos] = '\0';
    *pos_io = pos;
    return true;
}

int
pgy_exec_probe_argv_silent(const char *const argv[])
{
    char cmdline[1024];
    size_t pos = 0;
    for (const char *const *p = argv; *p != NULL; p++) {
        if (pos > 0) {
            if (pos + 1 >= sizeof(cmdline))
                return -1;
            cmdline[pos++] = ' ';
            cmdline[pos] = '\0';
        }
        if (!pgy_win32_quote_arg(cmdline, sizeof(cmdline), &pos, *p))
            return -1;
    }
    cmdline[pos] = '\0';

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE nul = CreateFileA("NUL", GENERIC_WRITE,
                             FILE_SHARE_WRITE | FILE_SHARE_READ,
                             &sa, OPEN_EXISTING, 0, NULL);
    if (nul == INVALID_HANDLE_VALUE)
        return -1;

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = nul;
    si.hStdError = nul;

    BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (!ok) {
        CloseHandle(nul);
        return -1;
    }

    DWORD wait_rc = WaitForSingleObject(pi.hProcess, 5000);
    DWORD exit_code = 1;
    if (wait_rc == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 124);
        WaitForSingleObject(pi.hProcess, 1000);
        exit_code = 124;
    } else if (wait_rc == WAIT_OBJECT_0) {
        if (!GetExitCodeProcess(pi.hProcess, &exit_code))
            exit_code = 1;
    } else {
        exit_code = 1;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(nul);
    return (int)exit_code;
}
#else
int
pgy_exec_probe_argv_silent(const char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0)
        return -1;
    if (pid == 0) {
        FILE *devnull = fopen("/dev/null", "w");
        if (devnull != NULL) {
            dup2(fileno(devnull), STDOUT_FILENO);
            dup2(fileno(devnull), STDERR_FILENO);
            fclose(devnull);
        }
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
}
#endif

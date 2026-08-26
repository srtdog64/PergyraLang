#include "compiler_process.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#endif

static void
pgy_capture_append(unsigned char **output,
                   size_t *length,
                   size_t *capacity,
                   const unsigned char *chunk,
                   size_t chunk_length,
                   size_t max_output_bytes,
    int *capture_error)
{
    size_t needed;
    size_t next;
    unsigned char *grown;

    if (*capture_error != 0 || chunk_length == 0)
        return;
    if (*length > max_output_bytes
        || chunk_length > max_output_bytes - *length) {
        *capture_error = PGY_EXEC_CAPTURE_OUTPUT_LIMIT;
        return;
    }
    needed = *length + chunk_length + 1;
    if (needed > *capacity) {
        next = *capacity == 0 ? 4096 : *capacity;
        while (next < needed) {
            size_t candidate = next * 2;
            if (candidate < next || candidate > max_output_bytes + 1)
                candidate = max_output_bytes + 1;
            next = candidate;
        }
        grown = realloc(*output, next);
        if (grown == NULL) {
            *capture_error = PGY_EXEC_CAPTURE_ERROR;
            return;
        }
        *output = grown;
        *capacity = next;
    }
    memcpy(*output + *length, chunk, chunk_length);
    *length += chunk_length;
    (*output)[*length] = '\0';
}

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
    bool needs_quotes = arg != NULL && arg[0] == '\0';
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
pgy_exec_argv_capture_stdout(const char *const argv[],
                             size_t max_output_bytes,
                             unsigned int timeout_millis,
                             unsigned char **output,
                             size_t *output_length)
{
    char cmdline[32768];
    size_t pos = 0;
    size_t capacity = 0;
    int capture_error = 0;
    bool child_exited = false;
    bool pipe_closed = false;
    HANDLE read_pipe = NULL;
    HANDLE write_pipe = NULL;
    HANDLE job = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD exit_code = 1;
    ULONGLONG started;

    if (argv == NULL || argv[0] == NULL || output == NULL
        || output_length == NULL || max_output_bytes == 0
        || timeout_millis == 0
        || max_output_bytes == (size_t)-1)
        return PGY_EXEC_CAPTURE_ERROR;
    *output = NULL;
    *output_length = 0;
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
    job = CreateJobObjectA(NULL, NULL);
    if (job == NULL)
        return PGY_EXEC_CAPTURE_ERROR;
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
        ZeroMemory(&info, sizeof(info));
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                                     &info, sizeof(info))) {
            CloseHandle(job);
            return PGY_EXEC_CAPTURE_ERROR;
        }
    }
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)
        || !SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0)) {
        if (read_pipe != NULL) CloseHandle(read_pipe);
        if (write_pipe != NULL) CloseHandle(write_pipe);
        CloseHandle(job);
        return PGY_EXEC_CAPTURE_ERROR;
    }
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = write_pipe;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE,
                        CREATE_NO_WINDOW | CREATE_SUSPENDED,
                        NULL, NULL, &si, &pi)) {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        CloseHandle(job);
        return PGY_EXEC_CAPTURE_ERROR;
    }
    if (!AssignProcessToJobObject(job, pi.hProcess)
        || ResumeThread(pi.hThread) == (DWORD)-1) {
        TerminateProcess(pi.hProcess, 124);
        WaitForSingleObject(pi.hProcess, 1000);
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(job);
        return PGY_EXEC_CAPTURE_ERROR;
    }
    CloseHandle(write_pipe);
    started = GetTickCount64();
    for (;;) {
        unsigned char chunk[16384];
        DWORD available = 0;
        DWORD count = 0;
        if (!pipe_closed &&
            !PeekNamedPipe(read_pipe, NULL, 0, NULL, &available, NULL)) {
            if (GetLastError() == ERROR_BROKEN_PIPE)
                pipe_closed = true;
            else {
                capture_error = PGY_EXEC_CAPTURE_ERROR;
                break;
            }
        }
        if (!pipe_closed && available > 0) {
            DWORD wanted = available < sizeof(chunk)
                ? available : (DWORD)sizeof(chunk);
            if (!ReadFile(read_pipe, chunk, wanted, &count, NULL)) {
                capture_error = PGY_EXEC_CAPTURE_ERROR;
                break;
            }
            pgy_capture_append(output, output_length, &capacity, chunk,
                               (size_t)count, max_output_bytes,
                               &capture_error);
            if (capture_error != 0)
                break;
            continue;
        }
        if (child_exited)
            break;
        switch (WaitForSingleObject(pi.hProcess, 0)) {
        case WAIT_OBJECT_0:
            child_exited = true;
            continue;
        case WAIT_TIMEOUT:
            break;
        default:
            capture_error = PGY_EXEC_CAPTURE_ERROR;
            break;
        }
        if (capture_error != 0)
            break;
        if (GetTickCount64() - started >= timeout_millis) {
            capture_error = PGY_EXEC_CAPTURE_TIMEOUT;
            break;
        }
        Sleep(1);
    }
    CloseHandle(read_pipe);
    if (capture_error != 0) {
        TerminateJobObject(job, 124);
        WaitForSingleObject(pi.hProcess, 1000);
    } else if (!GetExitCodeProcess(pi.hProcess, &exit_code)) {
        capture_error = PGY_EXEC_CAPTURE_ERROR;
    } else if (exit_code > INT_MAX) {
        capture_error = PGY_EXEC_CAPTURE_CRASHED;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(job);
    if (capture_error != 0) {
        free(*output);
        *output = NULL;
        *output_length = 0;
        return capture_error;
    }
    return (int)exit_code;
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
static unsigned long long
pgy_capture_monotonic_millis(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return 0;
    return (unsigned long long)now.tv_sec * 1000ULL
        + (unsigned long long)now.tv_nsec / 1000000ULL;
}

int
pgy_exec_argv_capture_stdout(const char *const argv[],
                             size_t max_output_bytes,
                             unsigned int timeout_millis,
                             unsigned char **output,
                             size_t *output_length)
{
    int pipefd[2];
    pid_t pid;
    int status = 0;
    size_t capacity = 0;
    int capture_error = 0;
    bool child_exited = false;
    bool pipe_closed = false;
    unsigned long long started;

    if (argv == NULL || argv[0] == NULL || output == NULL
        || output_length == NULL || max_output_bytes == 0
        || timeout_millis == 0
        || max_output_bytes == (size_t)-1)
        return PGY_EXEC_CAPTURE_ERROR;
    *output = NULL;
    *output_length = 0;
    if (pipe(pipefd) != 0)
        return -1;
    pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    if (pid == 0) {
        (void)setpgid(0, 0);
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) < 0)
            _exit(127);
        close(pipefd[1]);
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    close(pipefd[1]);
    (void)setpgid(pid, pid);
    {
        int flags = fcntl(pipefd[0], F_GETFL, 0);
        if (flags < 0 || fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK) < 0)
            capture_error = PGY_EXEC_CAPTURE_ERROR;
    }
    started = pgy_capture_monotonic_millis();
    for (;;) {
        while (!pipe_closed && capture_error == 0) {
            unsigned char chunk[16384];
            ssize_t count = read(pipefd[0], chunk, sizeof(chunk));
            if (count > 0) {
                pgy_capture_append(output, output_length, &capacity, chunk,
                                   (size_t)count, max_output_bytes,
                                   &capture_error);
                continue;
            }
            if (count == 0) {
                pipe_closed = true;
                break;
            }
            if (errno == EINTR)
                continue;
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                capture_error = PGY_EXEC_CAPTURE_ERROR;
            break;
        }
        if (capture_error != 0)
            break;
        if (child_exited)
            break;
        {
            pid_t waited = waitpid(pid, &status, WNOHANG);
            if (waited == pid) {
                child_exited = true;
                (void)kill(-pid, SIGKILL);
                continue;
            }
            if (waited < 0 && errno != EINTR) {
                capture_error = PGY_EXEC_CAPTURE_ERROR;
                break;
            }
        }
        if (pgy_capture_monotonic_millis() - started >= timeout_millis) {
            capture_error = PGY_EXEC_CAPTURE_TIMEOUT;
            break;
        }
        if (pipe_closed) {
            (void)poll(NULL, 0, 10);
            continue;
        }
        {
            struct pollfd ready = { pipefd[0], POLLIN | POLLHUP, 0 };
            if (poll(&ready, 1, 10) < 0 && errno != EINTR) {
                capture_error = PGY_EXEC_CAPTURE_ERROR;
                break;
            }
        }
    }
    close(pipefd[0]);
    if (!child_exited) {
        (void)kill(-pid, SIGKILL);
        (void)kill(pid, SIGKILL);
        while (waitpid(pid, &status, 0) < 0) {
            if (errno != EINTR) {
                capture_error = PGY_EXEC_CAPTURE_ERROR;
                break;
            }
        }
    }
    if (capture_error != 0) {
        free(*output);
        *output = NULL;
        *output_length = 0;
        return capture_error;
    }
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    free(*output);
    *output = NULL;
    *output_length = 0;
    return PGY_EXEC_CAPTURE_CRASHED;
}

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

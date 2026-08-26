#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

int
main(int argc, char **argv)
{
#ifdef _WIN32
    if (getenv("PGY_CAPTURE_BOUNDARY_CLOSE_STDOUT") != NULL) {
        CloseHandle(GetStdHandle(STD_OUTPUT_HANDLE));
        Sleep(1000);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--grandchild") == 0) {
        Sleep(30000);
        return 0;
    }
    if (getenv("PGY_CAPTURE_BOUNDARY_DESCENDANT") != NULL) {
        char binary[MAX_PATH];
        char command[MAX_PATH + 32];
        STARTUPINFOA startup;
        PROCESS_INFORMATION process;
        DWORD length = GetModuleFileNameA(NULL, binary, sizeof(binary));
        if (length == 0 || length >= sizeof(binary)) return 2;
        if (snprintf(command, sizeof(command), "\"%s\" --grandchild", binary) < 0)
            return 2;
        ZeroMemory(&startup, sizeof(startup));
        ZeroMemory(&process, sizeof(process));
        startup.cb = sizeof(startup);
        if (!CreateProcessA(NULL, command, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                            NULL, NULL, &startup, &process)) return 2;
        CloseHandle(process.hProcess);
        CloseHandle(process.hThread);
    }
#else
    (void)argc;
    (void)argv;
    if (getenv("PGY_CAPTURE_BOUNDARY_CLOSE_STDOUT") != NULL) {
        close(STDOUT_FILENO);
        sleep(1);
        return 0;
    }
    if (getenv("PGY_CAPTURE_BOUNDARY_DESCENDANT") != NULL) {
        pid_t child = fork();
        if (child < 0) return 2;
        if (child == 0) {
            sleep(30);
            _exit(0);
        }
    }
#endif
    return 0;
}

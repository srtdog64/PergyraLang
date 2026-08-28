#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char **argv)
{
    const char *count_path = getenv("PGY_SELF_DRIVER_COUNT_FILE");
    FILE *count;
    char command[32];

    if (argc != 3
        || strcmp(argv[1], "--debug-session") != 0
        || argv[2][0] == '\0'
        || count_path == NULL || count_path[0] == '\0')
        return 2;
    count = fopen(count_path, "ab");
    if (count == NULL)
        return 3;
    if (fputs("1\n", count) == EOF || fclose(count) != 0)
        return 4;

    fputs("debug-session-shim\n(pgy-debug:1) ", stdout);
    fflush(stdout);
    if (fgets(command, sizeof(command), stdin) == NULL)
        return 5;
    command[strcspn(command, "\r\n")] = '\0';
    return strcmp(command, "q") == 0 ? 0 : 6;
}

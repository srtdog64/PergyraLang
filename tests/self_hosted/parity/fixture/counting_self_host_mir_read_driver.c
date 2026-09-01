#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
supported_mode(const char *mode)
{
    return strcmp(mode, "--emit-mir-diagnostic-verified") == 0
        || strcmp(mode, "--emit-mir-json-diagnostic-verified") == 0
        || strcmp(mode, "--emit-mir-json-verified") == 0;
}

int
main(int argc, char **argv)
{
    const char *count_path = getenv("PGY_SELF_DRIVER_COUNT_FILE");
    FILE *count;

    if (argc != 3 || !supported_mode(argv[1]) || argv[2][0] == '\0'
        || count_path == NULL || count_path[0] == '\0')
        return 2;
    count = fopen(count_path, "ab");
    if (count == NULL)
        return 3;
    if (fprintf(count, "%s\n", argv[1]) < 0 || fclose(count) != 0)
        return 4;
    if (printf("mir-read-shim:%s\n", argv[1]) < 0)
        return 5;
    return 0;
}

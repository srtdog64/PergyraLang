#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char **argv)
{
    const char *count_path = getenv("PGY_SELF_DRIVER_COUNT_FILE");
    const char *mutate_source = getenv("PGY_SELF_DRIVER_MUTATE_SOURCE");
    FILE *count;
    FILE *output;
    FILE *source;

    if (argc != 5
        || strcmp(argv[1], "--format-source-verified") != 0
        || argv[2][0] == '\0'
        || strcmp(argv[3], "-o") != 0
        || argv[4][0] == '\0'
        || count_path == NULL || count_path[0] == '\0')
        return 2;
    count = fopen(count_path, "ab");
    if (count == NULL)
        return 3;
    if (fputs("1\n", count) == EOF || fclose(count) != 0)
        return 4;

    output = fopen(argv[4], "wb");
    if (output == NULL)
        return 5;
    if (fputs("func Main() -> Void\n{\n}\n", output) == EOF
        || fclose(output) != 0)
        return 6;
    if (mutate_source != NULL && mutate_source[0] != '\0') {
        source = fopen(argv[2], "wb");
        if (source == NULL)
            return 7;
        if (fputs("func ConcurrentEdit() -> Void\n{\n}\n", source) == EOF
            || fclose(source) != 0)
            return 8;
    }
    return 0;
}

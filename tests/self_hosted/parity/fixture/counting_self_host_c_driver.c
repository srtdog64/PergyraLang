#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char **argv)
{
    const char *count_path = getenv("PGY_SELF_DRIVER_COUNT_FILE");
    FILE *count;
    FILE *output;

    if (argc != 4
        || strcmp(argv[1], "--emit-c-artifact-verified") != 0
        || count_path == NULL || count_path[0] == '\0')
        return 2;
    count = fopen(count_path, "ab");
    if (count == NULL)
        return 3;
    if (fputs("1\n", count) == EOF || fclose(count) != 0)
        return 4;

    output = fopen(argv[3], "wb");
    if (output == NULL)
        return 5;
    if (fputs("#include <stdio.h>\n"
              "int main(void) { puts(\"self-host-shim\"); return 0; }\n",
              output) == EOF
        || fclose(output) != 0) {
        return 6;
    }
    return 0;
}

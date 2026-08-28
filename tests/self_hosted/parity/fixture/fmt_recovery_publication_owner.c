#include "../../../../src/compiler/fmt.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char original[] = "func Original() -> Void {}\n";
static const char concurrent[] = "func ConcurrentEdit() -> Void {}\n";
static const char formatted[] = "func Original() -> Void\n{\n}\n";

static int
write_text(const char *path, const char *text)
{
    FILE *stream = fopen(path, "wb");
    size_t length = strlen(text);
    int ok = stream != NULL && fwrite(text, 1, length, stream) == length;

    if (stream != NULL && fclose(stream) != 0)
        ok = 0;
    return ok;
}

char *
driver_self_host_source_identity_path_dup(const char *source_path)
{
    size_t length = strlen(source_path);
    char *copy = malloc(length + 1u);

    if (copy != NULL)
        memcpy(copy, source_path, length + 1u);
    return copy;
}

int
driver_materialize_self_host_format_artifact(const char *launcher_path,
                                             const char *source_path,
                                             const char *output_path)
{
    (void)launcher_path;
    (void)source_path;
    return write_text(output_path, formatted) ? 0 : 1;
}

void
pgy_path_replace_test_after_precheck(const char *dst_path)
{
    if (!write_text(dst_path, concurrent))
        exit(20);
}

bool
pgy_path_replace_test_rollback_enabled(void)
{
    return false;
}

int
main(int argc, char **argv)
{
    char *fmt_argv[3];

    if (argc != 2 || !write_text(argv[1], original))
        return 2;
    fmt_argv[0] = "fmt";
    fmt_argv[1] = argv[1];
    fmt_argv[2] = "--write";
    return driver_run_fmt_command(argv[0], 3, fmt_argv);
}

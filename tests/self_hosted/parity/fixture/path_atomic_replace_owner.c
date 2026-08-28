#include "../../../../src/compiler/path_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static const char expected[] = "func Original() -> Void {}\n";
static const char concurrent[] = "func ConcurrentEdit() -> Void {}\n";
static const char formatted[] = "func Original() -> Void\n{\n}\n";

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

static int
path_contains(const char *path, const char *expected_content)
{
    char *actual = path_read_file(path);
    int matches = actual != NULL && strcmp(actual, expected_content) == 0;

    free(actual);
    return matches;
}

int
main(int argc, char **argv)
{
    char *preserved;
    PathReplaceFileResult result;

    if (argc != 4)
        return 2;
    if (!write_text(argv[1], expected) || !write_text(argv[2], formatted))
        return 3;
    result = path_replace_file_atomic_if_unchanged(
        argv[2], argv[1], argv[3], expected);
    if (result != PATH_REPLACE_RECOVERY_REQUIRED)
        return 4;
    preserved = path_read_file(argv[1]);
    if (preserved == NULL || strcmp(preserved, formatted) != 0) {
        free(preserved);
        return 5;
    }
    free(preserved);
    if (!path_contains(argv[2], concurrent) &&
        !path_contains(argv[3], concurrent))
        return 6;
    return 0;
}

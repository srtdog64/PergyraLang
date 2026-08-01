#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
append_count(const char *value)
{
    const char *path = getenv("PGY_SELF_DRIVER_COUNT_FILE");
    FILE *out;

    if (path == NULL || path[0] == '\0')
        return 2;
    out = fopen(path, "ab");
    if (out == NULL)
        return 3;
    if (fputs(value, out) == EOF || fclose(out) != 0)
        return 4;
    return 0;
}

static int
write_text(const char *path, const char *text)
{
    FILE *out = fopen(path, "wb");
    if (out == NULL)
        return 5;
    if (fputs(text, out) == EOF || fclose(out) != 0)
        return 6;
    return 0;
}

int
main(int argc, char **argv)
{
    const char *mode = getenv("PGY_SELF_DRIVER_LLVM_MODE");
    int rc;

    if (argc != 4)
        return 2;
    if (mode == NULL)
        mode = "ok";
    if (strcmp(argv[1], "--emit-mir-json-verified") == 0) {
        rc = append_count("producer\n");
        if (rc != 0)
            return rc;
        if (strcmp(mode, "producer-fail") == 0)
            return 7;
        return write_text(argv[3], "{}\n");
    }
    if (strcmp(argv[1], "--mir-json-backend=llvm") != 0)
        return 8;
    rc = append_count("backend\n");
    if (rc != 0)
        return rc;
    if (strcmp(mode, "backend-fail") == 0)
        return 9;
    if (strcmp(mode, "malformed") == 0)
        return write_text(argv[3], "not llvm ir\n");
    if (strcmp(mode, "runtime-ref") == 0) {
        return write_text(
            argv[3],
            "declare void @pgy_forbidden()\n"
            "define i32 @main() { ret i32 0 }\n");
    }
    return write_text(
        argv[3],
        "@.msg = private constant [20 x i8] c\"self-host-llvm-shim\\00\"\n"
        "declare i32 @puts(ptr)\n"
        "define i32 @main() {\n"
        "  %r = call i32 @puts(ptr @.msg)\n"
        "  ret i32 0\n"
        "}\n");
}

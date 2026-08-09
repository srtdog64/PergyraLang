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
write_text(const char *path, const char *value)
{
    FILE *out = fopen(path, "wb");

    if (out == NULL)
        return 5;
    if (fputs(value, out) == EOF || fclose(out) != 0)
        return 6;
    return 0;
}

int
main(int argc, char **argv)
{
    int rc;

    if (argc == 4
        && strcmp(argv[1], "--emit-c-artifact-verified") == 0) {
        rc = append_count("c\n");
        if (rc != 0)
            return rc;
        return write_text(
            argv[3],
            "#include <stdio.h>\n"
            "int main(void) { puts(\"package-self-host-shim\"); return 0; }\n");
    }
    if (argc != 5 || strcmp(argv[3], "-o") != 0)
        return 7;
    if (strcmp(argv[1], "--emit-mir-json-verified") == 0) {
        rc = append_count("mir\n");
        if (rc != 0)
            return rc;
        return write_text(argv[4], "{}\n");
    }
    if (strcmp(argv[1], "--mir-json-backend=llvm") == 0) {
        rc = append_count("llvm\n");
        if (rc != 0)
            return rc;
        return write_text(
            argv[4],
            "@.msg = private constant [23 x i8] c\"package-self-host-shim\\00\"\n"
            "declare i32 @puts(ptr)\n"
            "define i32 @main() {\n"
            "  %r = call i32 @puts(ptr @.msg)\n"
            "  ret i32 0\n"
            "}\n");
    }
    return 8;
}

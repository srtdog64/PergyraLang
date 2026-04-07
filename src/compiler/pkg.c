/*
 * pgy init / pgy install — package manager
 *
 * pgy.toml format:
 *   [package]
 *   name = "my-app"
 *   version = "0.1.0"
 *   pergyra = "1.0"
 *
 *   [dependencies]
 *   # name = "version"
 *
 *   [dev-dependencies]
 *   # name = "version"
 */

#include "pkg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

static bool
file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

int
driver_run_pkg_init(int argc, char *argv[])
{
    const char *name = "my-project";
    if (argc > 0 && argv[0][0] != '-') {
        name = argv[0];
    }

    if (file_exists("pgy.toml")) {
        fprintf(stderr, "pgy init: pgy.toml already exists\n");
        return 1;
    }

    FILE *f = fopen("pgy.toml", "w");
    if (!f) {
        fprintf(stderr, "pgy init: cannot create pgy.toml\n");
        return 1;
    }

    fprintf(f,
        "[package]\n"
        "name = \"%s\"\n"
        "version = \"0.1.0\"\n"
        "pergyra = \"1.0\"\n"
        "entry = \"main.pgy\"\n"
        "\n"
        "[dependencies]\n"
        "# example = \"0.1.0\"\n"
        "\n"
        "[dev-dependencies]\n"
        "# test-utils = \"0.1.0\"\n",
        name);
    fclose(f);

    printf("pgy init: created pgy.toml for '%s'\n", name);

    /* Create main.pgy if it doesn't exist */
    if (!file_exists("main.pgy")) {
        FILE *m = fopen("main.pgy", "w");
        if (m) {
            fprintf(m,
                "func Main() -> Void\n"
                "{\n"
                "    Log(\"Hello, %s!\");\n"
                "}\n",
                name);
            fclose(m);
            printf("pgy init: created main.pgy\n");
        }
    }

    return 0;
}

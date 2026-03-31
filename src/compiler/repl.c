/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "repl.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

#include "driver_app.h"

static void
repl_tmp_path(char *out, size_t out_size, const char *ext)
{
    const char *tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL) tmpdir = getenv("TMP");
    if (tmpdir == NULL) tmpdir = getenv("TEMP");
#ifdef _WIN32
    if (tmpdir == NULL) tmpdir = ".";
#else
    if (tmpdir == NULL) tmpdir = "/tmp";
#endif

    static unsigned repl_salt = 0;
    if (repl_salt == 0)
        repl_salt = (unsigned)time(NULL) ^ (unsigned)getpid();

    snprintf(out, out_size, "%s/pgy_repl_%u_%x%s",
             tmpdir, (unsigned)getpid(), repl_salt, ext);
}

static bool
repl_line_is_decl(const char *line)
{
    static const char *prefixes[] = {
        "func ", "struct ", "class ", "ability ", "role ",
        "party ", "systemic ", "world ", "actor ", "enum ",
        "namespace ", "import ", "event ", NULL
    };

    for (size_t i = 0; prefixes[i] != NULL; i++) {
        if (strncmp(line, prefixes[i], strlen(prefixes[i])) == 0)
            return true;
    }
    return false;
}

int
repl_run(void)
{
    printf("Pergyra REPL v0.1 — type 'exit' to quit\n");

    char decls[16384] = "";
    char line[2048];

    while (1) {
        printf("pgy> ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL)
            break;

        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[--len] = '\0';
        if (len > 0 && line[len - 1] == '\r')
            line[--len] = '\0';

        if (len == 0)
            continue;
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0)
            break;

        if (repl_line_is_decl(line)) {
            char block[4096];
            snprintf(block, sizeof(block), "%s\n", line);

            int depth = 0;
            for (const char *p = line; *p; p++) {
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
            }

            while (depth > 0) {
                printf("...  ");
                fflush(stdout);
                if (fgets(line, sizeof(line), stdin) == NULL)
                    break;
                size_t l = strlen(line);
                if (l > 0 && line[l - 1] == '\n')
                    line[--l] = '\0';
                strncat(block, line, sizeof(block) - strlen(block) - 2);
                strcat(block, "\n");
                for (const char *p = line; *p; p++) {
                    if (*p == '{') depth++;
                    else if (*p == '}') depth--;
                }
            }

            strncat(decls, block, sizeof(decls) - strlen(decls) - 1);
            printf("  (defined)\n");
            continue;
        }

        char tmp_source[32768];
        snprintf(tmp_source, sizeof(tmp_source),
                 "%s\nfunc Main() -> Void {\n    %s\n}\n", decls, line);

        char tmp_pgy[512];
        char tmp_c[512];
        char tmp_exe[512];
        repl_tmp_path(tmp_pgy, sizeof(tmp_pgy), ".pgy");
        repl_tmp_path(tmp_c, sizeof(tmp_c), ".c");
#ifdef _WIN32
        repl_tmp_path(tmp_exe, sizeof(tmp_exe), ".exe");
#else
        repl_tmp_path(tmp_exe, sizeof(tmp_exe), "");
#endif

        FILE *f = fopen(tmp_pgy, "w");
        if (f == NULL) {
            fprintf(stderr, "  error: cannot create temp file\n");
            continue;
        }
        fputs(tmp_source, f);
        fclose(f);

        DriverFlags rf;
        memset(&rf, 0, sizeof(rf));
        rf.source_path = tmp_pgy;
        rf.do_run = true;
        driver_run_pipeline(&rf);

        remove(tmp_pgy);
        remove(tmp_c);
        remove(tmp_exe);
    }

    printf("Bye!\n");
    return 0;
}

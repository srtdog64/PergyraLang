/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "driver_scaffold.h"
#include "driver_scaffold_internal.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>
#endif

#include "../common/string_compat.h"

static bool
scaffold_has_suffix(const char *value, const char *suffix)
{
    size_t value_len;
    size_t suffix_len;

    if (value == NULL || suffix == NULL)
        return false;

    value_len = strlen(value);
    suffix_len = strlen(suffix);
    if (value_len < suffix_len)
        return false;
    return strcmp(value + value_len - suffix_len, suffix) == 0;
}

char *
scaffold_base_name_dup(const char *path)
{
    const char *base = path;
    const char *slash;
    const char *dot;
    size_t len;

    if (path == NULL)
        return NULL;

    slash = strrchr(path, '/');
    if (slash != NULL && slash[1] != '\0')
        base = slash + 1;
    dot = strrchr(base, '.');
    len = (dot != NULL && dot > base) ? (size_t)(dot - base) : strlen(base);
    return pergyra_strndup(base, len);
}

int
scaffold_mkdir_p(const char *path)
{
    char *buf;
    size_t len;

    if (path == NULL || *path == '\0')
        return 0;

    len = strlen(path);
    buf = pergyra_strdup(path);
    if (buf == NULL)
        return 1;

#ifdef _WIN32
#define PGY_MKDIR(path_) _mkdir(path_)
#else
#define PGY_MKDIR(path_) mkdir((path_), 0777)
#endif

    for (size_t i = 1; i < len; i++) {
        if (buf[i] == '/') {
            buf[i] = '\0';
            if (buf[0] != '\0' && PGY_MKDIR(buf) != 0 && errno != EEXIST) {
                fprintf(stderr, "pgy: failed to create directory '%s': %s\n",
                    buf, strerror(errno));
                free(buf);
                return 1;
            }
            buf[i] = '/';
        }
    }

    if (PGY_MKDIR(buf) != 0 && errno != EEXIST) {
        fprintf(stderr, "pgy: failed to create directory '%s': %s\n",
            buf, strerror(errno));
        free(buf);
        return 1;
    }

#undef PGY_MKDIR

    free(buf);
    return 0;
}

static int
scaffold_ensure_parent_dir(const char *path)
{
    char *dir;
    char *slash;
    int rc;

    if (path == NULL)
        return 1;

    dir = pergyra_strdup(path);
    if (dir == NULL)
        return 1;

    slash = strrchr(dir, '/');
    if (slash == NULL) {
        free(dir);
        return 0;
    }
    *slash = '\0';
    rc = scaffold_mkdir_p(dir);
    free(dir);
    return rc;
}

int
scaffold_write_file(const char *path, const char *content)
{
    FILE *fp;

    if (path == NULL || content == NULL)
        return 1;
    if (scaffold_ensure_parent_dir(path) != 0)
        return 1;

    fp = fopen(path, "rb");
    if (fp != NULL) {
        fclose(fp);
        fprintf(stderr, "pgy: refusing to overwrite existing file '%s'\n", path);
        return 1;
    }

    fp = fopen(path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "pgy: failed to write '%s': %s\n", path, strerror(errno));
        return 1;
    }

    if (fputs(content, fp) == EOF) {
        fprintf(stderr, "pgy: failed to write '%s'\n", path);
        fclose(fp);
        return 1;
    }

    fclose(fp);
    return 0;
}

static char *
scaffold_file_path_dup(const char *target)
{
    size_t len;
    char *path;

    if (target == NULL)
        return NULL;
    if (scaffold_has_suffix(target, ".pgy"))
        return pergyra_strdup(target);

    len = strlen(target) + 5;
    path = malloc(len);
    if (path == NULL)
        return NULL;
    snprintf(path, len, "%s.pgy", target);
    return path;
}

static int
scaffold_subject_file(const char *target)
{
    char *path = scaffold_file_path_dup(target);
    char *name = scaffold_base_name_dup(target);
    char content[2048];
    int rc;

    if (path == NULL || name == NULL) {
        free(path);
        free(name);
        return 1;
    }

    snprintf(content, sizeof(content),
        "subject %s\n"
        "{\n"
        "    let state: Int;\n"
        "\n"
        "    func Tick(self) -> Void\n"
        "    {\n"
        "        state = state + 1;\n"
        "    }\n"
        "\n"
        "    action Step(self) -> Void\n"
        "    {\n"
        "        Tick();\n"
        "    }\n"
        "}\n",
        name);

    rc = scaffold_write_file(path, content);
    if (rc == 0)
        printf("pgy: scaffolded subject -> %s\n", path);
    free(path);
    free(name);
    return rc;
}

static int
scaffold_class_file(const char *target)
{
    char *path = scaffold_file_path_dup(target);
    char *name = scaffold_base_name_dup(target);
    char content[2048];
    int rc;

    if (path == NULL || name == NULL) {
        free(path);
        free(name);
        return 1;
    }

    snprintf(content, sizeof(content),
        "class %s\n"
        "{\n"
        "    let label: String;\n"
        "    let bonus: Int;\n"
        "\n"
        "    func Summary(self) -> String\n"
        "    {\n"
        "        return label + \" (+\" + ToString(bonus) + \")\";\n"
        "    }\n"
        "}\n",
        name);

    rc = scaffold_write_file(path, content);
    if (rc == 0)
        printf("pgy: scaffolded class -> %s\n", path);
    free(path);
    free(name);
    return rc;
}

static int
scaffold_vessel_file(const char *target)
{
    char *path = scaffold_file_path_dup(target);
    char *name = scaffold_base_name_dup(target);
    char content[2048];
    int rc;

    if (path == NULL || name == NULL) {
        free(path);
        free(name);
        return 1;
    }

    snprintf(content, sizeof(content),
        "vessel %s\n"
        "{\n"
        "    current: Int;\n"
        "\n"
        "    func Reset(self) -> Void\n"
        "    {\n"
        "        current = 0;\n"
        "    }\n"
        "}\n",
        name);

    rc = scaffold_write_file(path, content);
    if (rc == 0)
        printf("pgy: scaffolded vessel -> %s\n", path);
    free(path);
    free(name);
    return rc;
}

static int
scaffold_object_file(const char *target)
{
    char *path = scaffold_file_path_dup(target);
    char *name = scaffold_base_name_dup(target);
    char content[2048];
    int rc;

    if (path == NULL || name == NULL) {
        free(path);
        free(name);
        return 1;
    }

    snprintf(content, sizeof(content),
        "object %s\n"
        "{\n"
        "    label: String;\n"
        "    value: Int;\n"
        "\n"
        "    func Summary(self) -> String\n"
        "    {\n"
        "        return label + \":\" + ToString(value);\n"
        "    }\n"
        "}\n",
        name);

    rc = scaffold_write_file(path, content);
    if (rc == 0)
        printf("pgy: scaffolded object -> %s\n", path);
    free(path);
    free(name);
    return rc;
}

static int
scaffold_dto_file(const char *target)
{
    char *path = scaffold_file_path_dup(target);
    char *name = scaffold_base_name_dup(target);
    char content[1024];
    int rc;

    if (path == NULL || name == NULL) {
        free(path);
        free(name);
        return 1;
    }

    snprintf(content, sizeof(content),
        "tobject %s\n"
        "{\n"
        "    label: String;\n"
        "    value: Int;\n"
        "}\n",
        name);

    rc = scaffold_write_file(path, content);
    if (rc == 0)
        printf("pgy: scaffolded tobject -> %s\n", path);
    free(path);
    free(name);
    return rc;
}

static int
scaffold_zone_file(const char *target)
{
    char *path = scaffold_file_path_dup(target);
    char *name = scaffold_base_name_dup(target);
    char content[2048];
    int rc;

    if (path == NULL || name == NULL) {
        free(path);
        free(name);
        return 1;
    }

    snprintf(content, sizeof(content),
        "zone %s\n"
        "{\n"
        "    shared tick: Int = 0\n"
        "\n"
        "    func Step(self) -> Void\n"
        "    {\n"
        "        tick = tick + 1;\n"
        "        Log(ToString(tick));\n"
        "    }\n"
        "}\n",
        name);

    rc = scaffold_write_file(path, content);
    if (rc == 0)
        printf("pgy: scaffolded zone -> %s\n", path);
    free(path);
    free(name);
    return rc;
}

static int
scaffold_world_file(const char *target)
{
    char *path = scaffold_file_path_dup(target);
    char *name = scaffold_base_name_dup(target);
    char content[2048];
    int rc;

    if (path == NULL || name == NULL) {
        free(path);
        free(name);
        return 1;
    }

    snprintf(content, sizeof(content),
        "world %s\n"
        "{\n"
        "    shared tick: Int = 0\n"
        "\n"
        "    func Step(self) -> Void\n"
        "    {\n"
        "        tick = tick + 1;\n"
        "        Log(ToString(tick));\n"
        "    }\n"
        "}\n",
        name);

    rc = scaffold_write_file(path, content);
    if (rc == 0)
        printf("pgy: scaffolded world -> %s\n", path);
    free(path);
    free(name);
    return rc;
}

int
driver_run_scaffold_command(int argc, char *argv[])
{
    const char *verb;
    const char *kind;
    const char *target;

    verb = (argc > 0) ? argv[0] : "scaffold";
    if (argc < 3) {
        fprintf(stderr,
            "Usage:\n"
            "  pgy scaffold <subject|class|vessel|object|tobject|zone|world|simulator|project> <target>\n"
            "  pgy new <project-dir>\n"
            "\n"
            "Project design order:\n"
            "  intent -> world -> zone -> subject\n"
            "\n"
            "Host kinds:\n"
            "  subject  = active host / who performs the contract\n"
            "  class    = passive tool or thing with hosted func\n"
            "  object   = passive view or state target\n"
            "  tobject  = transfer object (boundary data)\n");
        return 1;
    }

    kind = argv[1];
    target = argv[2];

    if (strcmp(kind, "subject") == 0)
        return scaffold_subject_file(target);
    if (strcmp(kind, "class") == 0)
        return scaffold_class_file(target);
    if (strcmp(kind, "vessel") == 0)
        return scaffold_vessel_file(target);
    if (strcmp(kind, "object") == 0)
        return scaffold_object_file(target);
    if (strcmp(kind, "tobject") == 0)
        return scaffold_dto_file(target);
    if (strcmp(kind, "zone") == 0)
        return scaffold_zone_file(target);
    if (strcmp(kind, "world") == 0)
        return scaffold_world_file(target);
    if (strcmp(kind, "simulator") == 0)
        return scaffold_simulator_dir(target);
    if (strcmp(kind, "project") == 0)
        return scaffold_project_dir(target);

    fprintf(stderr, "pgy: unknown scaffold kind '%s' for '%s'\n", kind, verb);
    return 1;
}


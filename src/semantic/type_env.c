/*
 * Copyright (c) 2025 Pergyra Language Project
 * Type environment implementation
 */

#include "type_system.h"

#include <stdlib.h>
#include <string.h>

TypeEnv *
type_env_create(TypeEnv *parent)
{
    TypeEnv *env = calloc(1, sizeof(TypeEnv));
    if (env == NULL)
        return NULL;
    env->parent = parent;
    return env;
}

void
type_env_destroy(TypeEnv *env)
{
    if (env == NULL)
        return;
    free(env->variables);
    free(env->types);
    free(env);
}

void
type_env_add_variable(TypeEnv *env, const char *name, Type *type)
{
    size_t n = env->var_count;
    void *grown = realloc(env->variables, (n + 1) * sizeof(*env->variables));
    if (grown == NULL)
        return;
    env->variables = grown;
    env->variables[n].name = (char *)name;
    env->variables[n].type = type;
    env->var_count++;
}

Type *
type_env_lookup_variable(TypeEnv *env, const char *name)
{
    TypeEnv *cur = env;
    while (cur != NULL) {
        for (size_t i = 0; i < cur->var_count; i++) {
            if (strcmp(cur->variables[i].name, name) == 0)
                return cur->variables[i].type;
        }
        cur = cur->parent;
    }
    return NULL;
}

void
type_env_add_type(TypeEnv *env, const char *name, Type *type)
{
    size_t n = env->type_count;
    void *grown = realloc(env->types, (n + 1) * sizeof(*env->types));
    if (grown == NULL)
        return;
    env->types = grown;
    env->types[n].name = (char *)name;
    env->types[n].type = type;
    env->type_count++;
}

Type *
type_env_lookup_type(TypeEnv *env, const char *name)
{
    TypeEnv *cur = env;
    while (cur != NULL) {
        for (size_t i = 0; i < cur->type_count; i++) {
            if (strcmp(cur->types[i].name, name) == 0)
                return cur->types[i].type;
        }
        cur = cur->parent;
    }
    return NULL;
}

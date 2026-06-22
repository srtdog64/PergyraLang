/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifndef PGY_DRIVER_SCAFFOLD_INTERNAL_H
#define PGY_DRIVER_SCAFFOLD_INTERNAL_H

char *scaffold_base_name_dup(const char *path);
char *scaffold_identifier_name_dup(const char *path);
int scaffold_mkdir_p(const char *path);
int scaffold_write_file(const char *path, const char *content);

int scaffold_simulator_dir(const char *target);
int scaffold_project_dir(const char *target);

#endif /* PGY_DRIVER_SCAFFOLD_INTERNAL_H */

/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "module_loader.h"

#include "import_resolver.h"

ASTNode *
module_loader_load_program(const char *source_path, char **error_message)
{
    return import_resolver_load_program(source_path, error_message);
}

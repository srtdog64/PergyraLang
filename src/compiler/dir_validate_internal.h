#ifndef PGY_DIR_VALIDATE_INTERNAL_H
#define PGY_DIR_VALIDATE_INTERNAL_H

#include <stdbool.h>

#include "dir.h"

char *dir_validate_strdup_fmt(const char *fmt, ...);

bool dir_domain_topology_is_projection(DIRDomainTopologyKind kind);
bool dir_validate_domain_topology(const DIRProgram *dir,
                                  char **error_message);
bool dir_validate_domain_runtime_facts(const DIRProgram *dir,
                                       char **error_message);
bool dir_validate_intents(const DIRProgram *dir, char **error_message);

#endif /* PGY_DIR_VALIDATE_INTERNAL_H */

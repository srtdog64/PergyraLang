#ifndef PERGYRA_ARRAY_STORAGE_ELEMENT_LIFETIME_OWNER_H
#define PERGYRA_ARRAY_STORAGE_ELEMENT_LIFETIME_OWNER_H
#include "type_system.h"
typedef struct SemanticContext SemanticContext;

/* Recursively plain value elements need no destructor or borrowed lifetime. */
bool semantic_array_storage_plain_element(const Type *type, SemanticContext *ctx);
#endif

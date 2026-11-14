#ifndef PERGYRA_COMPILER_AIR_VALIDATE_MACHINE_LAYER_H
#define PERGYRA_COMPILER_AIR_VALIDATE_MACHINE_LAYER_H

#include <stdbool.h>

typedef struct AIRProgram AIRProgram;

bool air_validate_machine_layer_site_inventory(const AIRProgram *air,
                                               char **error_message);

#endif

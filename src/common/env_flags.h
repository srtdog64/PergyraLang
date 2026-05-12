#ifndef PGY_ENV_FLAGS_H
#define PGY_ENV_FLAGS_H

#include <stdbool.h>

bool pgy_env_value_is_false(const char *value);
bool pgy_env_value_is_truthy(const char *value);

#endif /* PGY_ENV_FLAGS_H */

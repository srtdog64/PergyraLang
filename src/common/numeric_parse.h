#ifndef PGY_NUMERIC_PARSE_H
#define PGY_NUMERIC_PARSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool pgy_parse_int_prefix(const char *text, int *out);
bool pgy_parse_positive_int_prefix(const char *text, int *out);
bool pgy_parse_positive_int_strict(const char *text, int *out);
bool pgy_parse_size_prefix(const char *text, size_t *out);
bool pgy_parse_size_strict(const char *text, size_t *out);
bool pgy_parse_size_strict_allow_zero(const char *text, size_t *out);
bool pgy_parse_u64_strict_allow_zero(const char *text, uint64_t *out);

#endif /* PGY_NUMERIC_PARSE_H */

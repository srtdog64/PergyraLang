#ifndef PGY_OPERATOR_OVERLOAD_RULES_H
#define PGY_OPERATOR_OVERLOAD_RULES_H

#include "../lexer/lexer.h"

/* Native overload vocabulary only, not callable target resolution. Returned
 * strings are immutable and have process lifetime. Unknown tokens/names are
 * not overloads; an out-of-range enumeration index returns TOKEN_EOF. */
size_t pgy_operator_overload_count(void);
PgyTokenType pgy_operator_overload_at(size_t index);
const char *pgy_operator_overload_suffix(PgyTokenType op);
bool pgy_operator_method_name_matches(PgyTokenType op, const char *name);

#endif

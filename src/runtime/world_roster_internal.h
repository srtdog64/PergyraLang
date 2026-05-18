#ifndef PERGYRA_WORLD_ROSTER_INTERNAL_H
#define PERGYRA_WORLD_ROSTER_INTERNAL_H

#include "world_roster.h"

void world_roster_warn(const char* op, const char* reason);
uint64_t world_roster_now_ns(void);
void world_roster_sleep_ms(uint64_t timeoutMs);

#endif /* PERGYRA_WORLD_ROSTER_INTERNAL_H */

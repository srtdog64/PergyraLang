#include "type_checker_internal.h"

/*
 * Retired compatibility resolver quarantine.
 *
 * The recursive type-node evaluator has been removed from the beta path. This
 * owner remains as an explicit sentinel so inventory smoke tests can prevent
 * legacy resolver bodies from reappearing, but it no longer exports zero-only
 * telemetry counters.
 */

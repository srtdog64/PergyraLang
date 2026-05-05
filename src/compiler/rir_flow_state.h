#ifndef PERGYRA_RIR_FLOW_STATE_H
#define PERGYRA_RIR_FLOW_STATE_H

#include "rir.h"

RIRResourceState
rir_merge_states_for_kind(RIRResourceKind kind,
                          RIRResourceState a,
                          RIRResourceState b,
                          bool *conflict);

#endif

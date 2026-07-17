#ifndef PERGYRA_AIR_EVIDENCE_CERTIFICATE_H
#define PERGYRA_AIR_EVIDENCE_CERTIFICATE_H

#include <stdbool.h>
#include <stdint.h>

#include "air.h"
#include "air_verification_handle.h"

/*
 * AIR is the owner of evidence completeness and drift disposition.  The
 * projection planner may consume only this certificate, never reconstructing
 * evidence from AST/HIR/MIR strings in the backend.
 */
#define PGY_AIR_EVIDENCE_CERTIFICATE_SCHEMA "pgy.air.certificate.v1"

uint64_t pgy_air_evidence_certificate_fingerprint(const AIRProgram *air);

bool pgy_air_evidence_certificate_issue(AIRProgram *air,
                                         const char **error_out);

bool pgy_air_evidence_certificate_ready(const AIRProgram *air,
                                        const char **error_out);

#endif /* PERGYRA_AIR_EVIDENCE_CERTIFICATE_H */

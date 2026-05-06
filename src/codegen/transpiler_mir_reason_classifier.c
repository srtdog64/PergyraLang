/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR reason classification.
 */

#include "transpiler_mir_reason_classifier.h"
#include "../semantic/diag_codes.h"

#include <string.h>

typedef struct TranspilerMirReasonPattern {
    const char *needle;
    TranspilerMirReasonDiagnostic diag;
} TranspilerMirReasonPattern;

static const TranspilerMirReasonPattern kMirFunctionReasonPatterns[] = {
    {
        "unresolved identifier",
        { PGY_CODE_MIR_UNRESOLVED_LOCAL,
          PGY_CAUSE_MIR_LOCAL_UNRESOLVED,
          PGY_FIX_REPORT_COMPILER_BUG }
    },
    {
        "topology",
        { PGY_CODE_MIR_TOPOLOGY_INVALID,
          PGY_CAUSE_MIR_TOPOLOGY_INVALID,
          PGY_FIX_REPORT_COMPILER_BUG }
    },
    {
        "MIR contract invalid",
        { PGY_CODE_MIR_TOPOLOGY_INVALID,
          PGY_CAUSE_MIR_TOPOLOGY_INVALID,
          PGY_FIX_REPORT_COMPILER_BUG }
    },
    {
        "no routine",
        { PGY_CODE_MIR_TOPOLOGY_INVALID,
          PGY_CAUSE_MIR_TOPOLOGY_INVALID,
          PGY_FIX_REPORT_COMPILER_BUG }
    },
    {
        "no matching MIR routine",
        { PGY_CODE_MIR_TOPOLOGY_INVALID,
          PGY_CAUSE_MIR_TOPOLOGY_INVALID,
          PGY_FIX_REPORT_COMPILER_BUG }
    },
    {
        "no declaration AST",
        { PGY_CODE_MIR_TOPOLOGY_INVALID,
          PGY_CAUSE_MIR_TOPOLOGY_INVALID,
          PGY_FIX_REPORT_COMPILER_BUG }
    },
    {
        "unsupported MIR signature",
        { PGY_CODE_MIR_SIGNATURE_UNSUPPORTED,
          PGY_CAUSE_MIR_SIGNATURE_UNSUPPORTED,
          PGY_FIX_SIMPLIFY_FUNCTION_SIGNATURE }
    },
    {
        "wrong MIR kind",
        { PGY_CODE_MIR_SIGNATURE_UNSUPPORTED,
          PGY_CAUSE_MIR_SIGNATURE_UNSUPPORTED,
          PGY_FIX_SIMPLIFY_FUNCTION_SIGNATURE }
    },
    {
        "too many MIR SSA locals",
        { PGY_CODE_MIR_SSA_LIMIT,
          PGY_CAUSE_MIR_SSA_CAPACITY_EXCEEDED,
          PGY_FIX_REFACTOR_OR_RAISE_LIMIT }
    },
};

TranspilerMirReasonDiagnostic
transpiler_classify_mir_function_reason(const char *reason)
{
    TranspilerMirReasonDiagnostic diag = { 0 };

    if (reason == NULL)
        return diag;

    for (size_t i = 0;
         i < sizeof(kMirFunctionReasonPatterns) / sizeof(kMirFunctionReasonPatterns[0]);
         i++) {
        const TranspilerMirReasonPattern *pattern =
            &kMirFunctionReasonPatterns[i];
        if (strstr(reason, pattern->needle) != NULL)
            return pattern->diag;
    }

    return diag;
}

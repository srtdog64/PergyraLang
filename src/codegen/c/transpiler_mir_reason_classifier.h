#ifndef PGY_TRANSPILER_MIR_REASON_CLASSIFIER_H
#define PGY_TRANSPILER_MIR_REASON_CLASSIFIER_H

typedef struct TranspilerMirReasonDiagnostic {
    const char *code;
    const char *cause;
    const char *fix;
} TranspilerMirReasonDiagnostic;

TranspilerMirReasonDiagnostic
transpiler_classify_mir_function_reason(const char *reason);

#endif /* PGY_TRANSPILER_MIR_REASON_CLASSIFIER_H */

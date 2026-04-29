#include "driver_diag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../semantic/diag_codes.h"

static void
driver_json_emit_string(FILE *out, const char *s)
{
    fputc('"', out);
    if (s == NULL) {
        fputc('"', out);
        return;
    }
    for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; p++) {
        unsigned char c = *p;
        switch (c) {
        case '"':  fputs("\\\"", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\b': fputs("\\b", out);  break;
        case '\f': fputs("\\f", out);  break;
        case '\n': fputs("\\n", out);  break;
        case '\r': fputs("\\r", out);  break;
        case '\t': fputs("\\t", out);  break;
        default:
            if (c < 0x20)
                fprintf(out, "\\u%04x", c);
            else
                fputc((int)c, out);
        }
    }
    fputc('"', out);
}

static bool
driver_extract_line_col(const char *s, unsigned *line, unsigned *column)
{
    if (s == NULL || line == NULL)
        return false;
    const char *p = strstr(s, "line ");
    if (p == NULL)
        return false;
    p += 5;
    char *endp = NULL;
    unsigned long l = strtoul(p, &endp, 10);
    if (endp == p)
        return false;
    *line = (unsigned)l;
    if (column != NULL) {
        *column = 0;
        const char *q = strstr(endp, "column ");
        if (q != NULL) {
            q += 7;
            unsigned long c = strtoul(q, &endp, 10);
            if (endp != q)
                *column = (unsigned)c;
        }
    }
    return true;
}

void
driver_emit_single_diag_json_full(const char *stage, const char *code,
                                  const char *cause_ir,
                                  const char *fix_source,
                                  const char *message)
{
    FILE *out = stderr;
    unsigned line = 0, column = 0;
    bool have_loc = driver_extract_line_col(message, &line, &column);

    fputs("[{\"severity\":\"error\",\"stage\":", out);
    driver_json_emit_string(out, stage != NULL ? stage : "unknown");
    if (code != NULL) {
        fputs(",\"code\":", out);
        driver_json_emit_string(out, code);
    }
    if (cause_ir != NULL) {
        fputs(",\"cause_ir\":", out);
        driver_json_emit_string(out, cause_ir);
    }
    if (fix_source != NULL) {
        fputs(",\"fix_source\":", out);
        driver_json_emit_string(out, fix_source);
    }
    if (have_loc)
        fprintf(out, ",\"location\":{\"line\":%u,\"column\":%u}", line, column);
    else
        fputs(",\"location\":null", out);
    fputs(",\"message\":", out);
    driver_json_emit_string(out, message != NULL ? message : "");
    fputs("}]\n", out);
}

void
driver_emit_single_diag_json_with_code(const char *stage,
                                       const char *code,
                                       const char *message)
{
    driver_emit_single_diag_json_full(stage, code, NULL, NULL, message);
}

void
driver_emit_single_diag_json(const char *stage, const char *message)
{
    driver_emit_single_diag_json_full(stage, NULL, NULL, NULL, message);
}

const char *
driver_diag_code_from_message(const char *message)
{
    if (message == NULL)
        return NULL;
    if (strstr(message, PGY_CODE_LEX_INVALID_TOKEN) != NULL)
        return PGY_CODE_LEX_INVALID_TOKEN;
    if (strstr(message, PGY_CODE_PARSE_SYNTAX) != NULL)
        return PGY_CODE_PARSE_SYNTAX;
    if (strstr(message, PGY_CODE_AIR_INVARIANT_INVALID) != NULL)
        return PGY_CODE_AIR_INVARIANT_INVALID;
    if (strstr(message, PGY_CODE_DRIVER_RUNTIME_NONE_UNSUPPORTED) != NULL)
        return PGY_CODE_DRIVER_RUNTIME_NONE_UNSUPPORTED;
    return NULL;
}

const char *
driver_diag_cause_from_code(const char *code)
{
    if (code == NULL)
        return NULL;
    if (strcmp(code, PGY_CODE_LEX_INVALID_TOKEN) == 0)
        return PGY_CAUSE_LEX_INVALID_TOKEN;
    if (strcmp(code, PGY_CODE_PARSE_SYNTAX) == 0)
        return PGY_CAUSE_PARSE_UNEXPECTED_TOKEN;
    if (strcmp(code, PGY_CODE_AIR_INVARIANT_INVALID) == 0)
        return PGY_CAUSE_AIR_INVARIANT_INVALID;
    if (strcmp(code, PGY_CODE_DRIVER_RUNTIME_NONE_UNSUPPORTED) == 0)
        return PGY_CAUSE_DRIVER_RUNTIME_NONE_UNSUPPORTED;
    return NULL;
}

const char *
driver_diag_fix_from_code(const char *code)
{
    if (code == NULL)
        return NULL;
    if (strcmp(code, PGY_CODE_LEX_INVALID_TOKEN) == 0)
        return PGY_FIX_REMOVE_OR_ESCAPE_CHARACTER;
    if (strcmp(code, PGY_CODE_PARSE_SYNTAX) == 0)
        return PGY_FIX_CHECK_SYNTAX;
    if (strcmp(code, PGY_CODE_AIR_INVARIANT_INVALID) == 0)
        return PGY_FIX_REPORT_COMPILER_BUG;
    if (strcmp(code, PGY_CODE_DRIVER_RUNTIME_NONE_UNSUPPORTED) == 0)
        return PGY_FIX_USE_DEFAULT_RUNTIME_OR_REMOVE_RUNTIME_SURFACE;
    return NULL;
}

void
driver_emit_stage_fail(const DriverFlags *flags,
                       const char *stage,
                       const char *description,
                       const char *detail)
{
    const char *msg = (detail != NULL) ? detail : "out of memory";
    if (flags != NULL && flags->diag_format == DIAG_FORMAT_JSON) {
        const char *code = driver_diag_code_from_message(msg);
        const char *cause_ir = driver_diag_cause_from_code(code);
        const char *fix_source = driver_diag_fix_from_code(code);
        driver_emit_single_diag_json_full(driver_route_stage(stage, code),
                                          code,
                                          cause_ir,
                                          fix_source,
                                          msg);
    } else {
        fprintf(stderr, "pgy: %s: %s\n", description, msg);
    }
}

static bool
driver_format_air_authority_names(const AIRBoundaryNode *boundary,
                                  char *out,
                                  size_t out_size)
{
    size_t used = 0;
    bool emitted = false;

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    if (boundary == NULL || boundary->authority_names == NULL)
        return false;

    for (size_t i = 0; i < boundary->authority_name_count; i++) {
        const char *name = boundary->authority_names[i];
        int written;

        if (name == NULL || name[0] == '\0')
            continue;
        written = snprintf(out + used,
                           out_size - used,
                           "%s%s",
                           emitted ? ", " : "",
                           name);
        if (written < 0)
            return emitted;
        if ((size_t)written >= out_size - used) {
            out[out_size - 1] = '\0';
            return true;
        }
        used += (size_t)written;
        emitted = true;
    }
    return emitted;
}

static bool
driver_format_air_evidence_summary(const AIRBoundaryNode *boundary,
                                   char *out,
                                   size_t out_size)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    if (boundary == NULL)
        return false;

    written = snprintf(out,
                       out_size,
                       "evidence hir=%s hir_cfg=%s rir_boundary=%s rir_authority=%s",
                       boundary->hir_routine_evidence_name != NULL
                           ? boundary->hir_routine_evidence_name
                           : "<none>",
                       boundary->has_hir_cfg_evidence ? "yes" : "no",
                       boundary->rir_boundary_evidence_scope != NULL
                           ? boundary->rir_boundary_evidence_scope
                           : "<none>",
                       boundary->rir_authority_evidence_name != NULL
                           ? boundary->rir_authority_evidence_name
                           : "<none>");
    if (written < 0)
        return false;
    out[out_size - 1] = '\0';
    return true;
}

void
driver_emit_air_drift_fail(const DriverFlags *flags, const AIRProgram *air)
{
    const AIRDrift *drift = NULL;
    const AIRIntentNode *intent = NULL;
    const AIRBoundaryNode *boundary = NULL;
    const ASTNode *site = NULL;
    char message[4096];
    const char *code = PGY_CODE_SEM_INTENT_BOUNDARY_DRIFT;
    const char *cause_ir = PGY_CAUSE_INTENT_BOUNDARY_DRIFT;
    const char *fix_source = PGY_FIX_ALIGN_INTENT_BOUNDARY_SYNC;
    const char *reason = "intent orchestration and implementation boundary disagree on sync/async behavior";
    const char *fix = "align the intent step contract with the boundary or move the implementation through a matching boundary";
    char authority_names[256];
    char evidence_summary[512];
    char reason_with_authority[1024];
    char reason_with_evidence[1536];
    unsigned line = 0;
    unsigned column = 0;

    if (air != NULL && air->drift_count > 0)
        drift = &air->drifts[0];
    if (drift != NULL && drift->intent_index < air->intent_count)
        intent = &air->intents[drift->intent_index];
    if (drift != NULL && drift->boundary_index < air->boundary_count)
        boundary = &air->boundaries[drift->boundary_index];
    if (intent != NULL && intent->ast != NULL)
        site = intent->ast;
    else if (boundary != NULL)
        site = boundary->ast;
    if (site != NULL) {
        line = site->line;
        column = site->column;
    }
    if (drift != NULL && drift->kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING) {
        bool missing_hir = boundary != NULL
            && air_boundary_requires_hir_evidence(boundary)
            && !boundary->has_hir_cfg_evidence;
        bool missing_rir = boundary != NULL
            && air_boundary_requires_rir_evidence(boundary)
            && !boundary->has_rir_boundary_evidence;
        code = PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING;
        cause_ir = PGY_CAUSE_INTENT_BOUNDARY_EVIDENCE;
        fix_source = PGY_FIX_ALIGN_INTENT_BOUNDARY_EVIDENCE;
        if (missing_hir && missing_rir) {
            reason = "AIR strict-evidence mode could not reconcile the intent boundary with HIR CFG and RIR boundary evidence";
            fix = "align the intent boundary with lowering-visible HIR CFG and RIR boundary evidence or extend AIR synthesis for this valid boundary";
        } else if (missing_hir) {
            reason = "AIR strict-evidence mode could not reconcile the intent boundary with HIR CFG evidence";
            fix = "align the implementation boundary with a lowering-visible HIR CFG routine or extend AIR/HIR evidence synthesis for this valid boundary";
        } else if (missing_rir) {
            reason = "AIR strict-evidence mode could not reconcile the intent boundary with RIR boundary evidence";
            fix = "align the intent boundary with a lowering-visible zone/world boundary or extend AIR/RIR synthesis for this valid boundary";
        } else {
            reason = "AIR strict-evidence mode could not reconcile the intent boundary with required authority evidence";
            fix = "align the authorized participant with a lowering-visible authority fact or extend AIR/RIR authority evidence synthesis";
        }
        if (boundary != NULL
            && boundary->authority_required
            && !boundary->has_rir_authority_evidence
            && driver_format_air_authority_names(boundary,
                                                 authority_names,
                                                 sizeof(authority_names))) {
            snprintf(reason_with_authority,
                     sizeof(reason_with_authority),
                     "%s; expected authority participant(s): %s",
                     reason,
                     authority_names);
            reason = reason_with_authority;
        }
        if (driver_format_air_evidence_summary(boundary,
                                               evidence_summary,
                                               sizeof(evidence_summary))) {
            snprintf(reason_with_evidence,
                     sizeof(reason_with_evidence),
                     "%s; %s",
                     reason,
                     evidence_summary);
            reason = reason_with_evidence;
        }
    }

    snprintf(message, sizeof(message),
             "AIR intent/boundary drift at line %u, column %u: intent '%s' step '%s' expects %s boundary but implementation boundary '%s' is %s. Reason: %s. Fix: %s.",
             line,
             column,
             intent != NULL && intent->intent_owner != NULL ? intent->intent_owner : "<unknown>",
             intent != NULL && intent->step_name != NULL ? intent->step_name : "<unknown>",
             intent != NULL ? air_sync_class_name(intent->sync_class) : "unknown",
             boundary != NULL && boundary->source_name != NULL ? boundary->source_name : "<unknown>",
             boundary != NULL ? air_sync_class_name(boundary->sync_class) : "unknown",
             reason,
             fix);

    if (flags != NULL && flags->diag_format == DIAG_FORMAT_JSON) {
        driver_emit_single_diag_json_full("semantic",
                                          code,
                                          cause_ir,
                                          fix_source,
                                          message);
    } else {
        fprintf(stderr, "pgy: %s: %s\n", code, message);
    }
}

const char *
driver_route_stage(const char *default_stage, const char *code)
{
    if (code == NULL)
        return default_stage;
    if (strncmp(code, "PGY_MIR_", 8) == 0)
        return "mir_validation";
    if (strncmp(code, "PGY_AIR_", 8) == 0)
        return "air_verify";
    if (strncmp(code, "PGY_DRIVER_", 11) == 0)
        return "driver";
    if (strncmp(code, "PGY_C_", 6) == 0)
        return "c_codegen";
    if (strncmp(code, "PGY_LLVM_", 9) == 0)
        return "llvm_codegen";
    if (strncmp(code, "PGY_SEM_", 8) == 0)
        return "semantic";
    if (strncmp(code, "PGY_PARSE_", 10) == 0)
        return "parse";
    if (strncmp(code, "PGY_LEX_", 8) == 0)
        return "lex";
    return default_stage;
}

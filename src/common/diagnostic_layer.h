#ifndef PERGYRA_DIAGNOSTIC_LAYER_H
#define PERGYRA_DIAGNOSTIC_LAYER_H

/*
 * Tooling-facing abstraction layer for diagnostics.
 *
 * This keeps Pergyra's visible stack explicit without duplicating routing
 * tables in semantic, driver, JSON, or LSP output paths.
 */
typedef enum
{
    DIAG_LAYER_UNKNOWN,
    DIAG_LAYER_SYNTAX,
    DIAG_LAYER_TYPE,
    DIAG_LAYER_RESOURCE,
    DIAG_LAYER_CONCURRENCY,
    DIAG_LAYER_DOMAIN,
    DIAG_LAYER_BACKEND,
    DIAG_LAYER_DRIVER
} DiagnosticLayer;

const char *diagnostic_layer_name(DiagnosticLayer layer);
DiagnosticLayer diagnostic_layer_from_tags(const char *stage,
                                           const char *cause_ir,
                                           const char *code);

#endif /* PERGYRA_DIAGNOSTIC_LAYER_H */

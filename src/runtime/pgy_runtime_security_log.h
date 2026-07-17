#ifndef PGY_RUNTIME_SECURITY_LOG_H
#define PGY_RUNTIME_SECURITY_LOG_H

#include "pgy_runtime_linkage.h"

#include <stdio.h>

PGY_RT_DECL void
pgy_runtime_fprint_json_string(FILE *out, const char *text)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    const unsigned char *p;

    fputc('"', out);
    if (text == NULL)
        text = "";
    p = (const unsigned char *)text;
    while (*p != '\0') {
        switch (*p) {
        case '"':
            fputs("\\\"", out);
            break;
        case '\\':
            fputs("\\\\", out);
            break;
        case '\b':
            fputs("\\b", out);
            break;
        case '\f':
            fputs("\\f", out);
            break;
        case '\n':
            fputs("\\n", out);
            break;
        case '\r':
            fputs("\\r", out);
            break;
        case '\t':
            fputs("\\t", out);
            break;
        default:
            if (*p < 0x20)
                fprintf(out, "\\u%04x", (unsigned int)*p);
            else
                fputc((int)*p, out);
            break;
        }
        p++;
    }
    fputc('"', out);
}
#else
;
#endif


PGY_RT_DECL void
pgy_runtime_log_authority_failure(FILE *out,
                                  const char *code,
                                  const char *reason,
                                  const char *zone,
                                  const char *participant)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    fputs("{\"component\":\"authority\",\"event\":\"authority_validation_failed\","
          "\"code\":", out);
    pgy_runtime_fprint_json_string(out, code != NULL ? code : "unknown");
    fputs(",\"reason\":", out);
    pgy_runtime_fprint_json_string(out, reason != NULL ? reason : "unknown");
    fputs(",\"zone\":", out);
    pgy_runtime_fprint_json_string(out, zone != NULL ? zone : "<zone>");
    fputs(",\"participant\":", out);
    pgy_runtime_fprint_json_string(out,
                                   participant != NULL
                                       ? participant
                                       : "<participant>");
    fputs("}\n", out);
}
#else
;
#endif


#endif /* PGY_RUNTIME_SECURITY_LOG_H */

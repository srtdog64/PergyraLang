#include "semantic.h"

#include "../common/squiggle_class.h"
#include "diag_payload.h"

#include <stdio.h>

static void
json_emit_string(FILE *out, const char *text)
{
    fputc('"', out);
    if (text == NULL) {
        fputc('"', out);
        return;
    }
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++) {
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

void
semantic_result_print_json(const SemanticResult *result)
{
    FILE *out = stderr;
    if (result == NULL || result->diagnostic_count == 0) {
        fputs("[]\n", out);
        return;
    }

    fputc('[', out);
    for (size_t i = 0; i < result->diagnostic_count; i++) {
        Diagnostic *diagnostic = result->diagnostics[i];
        const char *severity = diagnostic->level == DIAG_ERROR
            ? "error"
            : (diagnostic->level == DIAG_WARNING ? "warning" : "advisory");
        SquiggleClass squiggle = squiggle_class_classify(
            diagnostic->level == DIAG_ERROR,
            diagnostic->layer, diagnostic->code);
        if (i > 0)
            fputc(',', out);
        fputs("{\"severity\":", out);
        json_emit_string(out, severity);
        fputs(",\"squiggleClass\":", out);
        json_emit_string(out, squiggle_class_name(squiggle));
        fputs(",\"stage\":\"semantic\",\"layer\":", out);
        json_emit_string(out, diagnostic_layer_name(diagnostic->layer));
        if (diagnostic->code != NULL) {
            fputs(",\"code\":", out);
            json_emit_string(out, diagnostic->code);
        }
        if (diagnostic->cause_ir != NULL) {
            fputs(",\"cause_ir\":", out);
            json_emit_string(out, diagnostic->cause_ir);
        }
        if (diagnostic->fix_source != NULL) {
            fputs(",\"fix_source\":", out);
            json_emit_string(out, diagnostic->fix_source);
        }
        fputs(",\"location\":{\"line\":", out);
        fprintf(out, "%u,\"column\":%u}",
                diagnostic->line, diagnostic->col);
        fputs(",\"message\":", out);
        json_emit_string(out,
            diagnostic->message != NULL ? diagnostic->message : "");
        if (diagnostic->payload != NULL) {
            bool wrote = false;
            fputs(",\"payload\":{", out);
#define PGY_JSON_PAYLOAD_FIELD(key, value) \
            do { \
                if ((value) != NULL) { \
                    if (wrote) \
                        fputc(',', out); \
                    json_emit_string(out, (key)); \
                    fputc(':', out); \
                    json_emit_string(out, (value)); \
                    wrote = true; \
                } \
            } while (0)
            PGY_JSON_PAYLOAD_FIELD("value_label",
                                   diagnostic->payload->value_label);
            PGY_JSON_PAYLOAD_FIELD("provenance_label",
                                   diagnostic->payload->provenance_label);
            PGY_JSON_PAYLOAD_FIELD("replacement_label",
                                   diagnostic->payload->replacement_label);
            PGY_JSON_PAYLOAD_FIELD("transfer_label",
                                   diagnostic->payload->transfer_label);
            PGY_JSON_PAYLOAD_FIELD("borrowed_name",
                                   diagnostic->payload->borrowed_name);
            PGY_JSON_PAYLOAD_FIELD("consumer_name",
                                   diagnostic->payload->consumer_name);
            PGY_JSON_PAYLOAD_FIELD("secondary_name",
                                   diagnostic->payload->secondary_name);
            PGY_JSON_PAYLOAD_FIELD("kind_label",
                                   diagnostic->payload->kind_label);
            PGY_JSON_PAYLOAD_FIELD("extra", diagnostic->payload->extra);
#undef PGY_JSON_PAYLOAD_FIELD
            fputc('}', out);
        }
        fputc('}', out);
    }
    fputs("]\n", out);
}

#!/usr/bin/env bash
# layered_diagnostics_contract_smoke.sh
#
# Regression: Pergyra diagnostics must expose the visible abstraction stack
# to tooling.  This keeps resource / concurrency / domain failures from being
# flattened into generic semantic errors in JSON/LSP consumers.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_layered_diag.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

CC_BIN="${CC:-}"
if [[ -z "$CC_BIN" ]]; then
    if command -v gcc >/dev/null 2>&1; then
        CC_BIN="gcc"
    elif command -v cc >/dev/null 2>&1; then
        CC_BIN="cc"
    fi
fi

if [[ -n "$CC_BIN" ]]; then
    cat >"$WORK_DIR/cc_probe.c" <<'C'
int main(void) { return 0; }
C
    if ! "$CC_BIN" "$WORK_DIR/cc_probe.c" -o "$WORK_DIR/cc_probe" >/dev/null 2>&1; then
        echo "[layered-diagnostics] SKIP runtime classifier harness (C compiler probe failed)"
        exit 0
    fi
    cat >"$WORK_DIR/layer_check.c" <<'C'
#include <stdio.h>
#include "src/common/diagnostic_layer.h"

static int expect_layer(const char *label,
                        const char *stage,
                        const char *cause,
                        const char *code,
                        DiagnosticLayer expected)
{
    DiagnosticLayer actual = diagnostic_layer_from_tags(stage, cause, code);
    if (actual != expected) {
        fprintf(stderr,
                "%s: expected %s, got %s\n",
                label,
                diagnostic_layer_name(expected),
                diagnostic_layer_name(actual));
        return 1;
    }
    return 0;
}

int main(void)
{
    int failures = 0;
    failures += expect_layer("parse", "parse", "parse:unexpected_token",
                             "PGY_PARSE_SYNTAX", DIAG_LAYER_SYNTAX);
    failures += expect_layer("cfg", "semantic",
                             "semantic:cfg:missing_return_path",
                             "PGY_SEM_MISSING_RETURN", DIAG_LAYER_TYPE);
    failures += expect_layer("assignability", "semantic",
                             "semantic:assignability_check",
                             "PGY_SEM_TYPE_MISMATCH", DIAG_LAYER_TYPE);
    failures += expect_layer("pin", "semantic",
                             "semantic:pin:await_boundary",
                             "PGY_SEM_PIN_AWAIT_BOUNDARY",
                             DIAG_LAYER_RESOURCE);
    failures += expect_layer("channel", "semantic",
                             "semantic:channel:payload",
                             "PGY_SEM_CHANNEL_PAYLOAD",
                             DIAG_LAYER_CONCURRENCY);
    failures += expect_layer("intent", "semantic",
                             "semantic:intent:boundary_evidence",
                             "PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING",
                             DIAG_LAYER_DOMAIN);
    failures += expect_layer("air", "air_verify",
                             "air:invariant",
                             "PGY_AIR_INVARIANT_INVALID",
                             DIAG_LAYER_BACKEND);
    failures += expect_layer("driver", "driver",
                             "driver:runtime_none",
                             "PGY_DRIVER_RUNTIME_NONE_UNSUPPORTED",
                             DIAG_LAYER_DRIVER);
    return failures != 0;
}
C
    "$CC_BIN" -std=c11 -Isrc -I. \
        "$WORK_DIR/layer_check.c" \
        "$ROOT_DIR/src/common/diagnostic_layer.c" \
        -o "$WORK_DIR/layer_check"
    "$WORK_DIR/layer_check"
else
    echo "[layered-diagnostics] SKIP runtime classifier harness (no C compiler)"
fi

echo "[layered-diagnostics] layer classifier harness done"

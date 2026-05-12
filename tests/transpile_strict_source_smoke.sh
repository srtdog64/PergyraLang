#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TRANSPILE_DIR="$ROOT_DIR/src/tests/transpile"
HELPERS="$TRANSPILE_DIR/test_transpile_helpers.cases.h"

if [[ ! -f "$HELPERS" ]]; then
    echo "[transpile-strict-source] missing helpers: $HELPERS" >&2
    exit 1
fi

if ! grep -Fq "lower_program_to_mir_strict" "$HELPERS"; then
    echo "[transpile-strict-source] strict MIR helper is missing" >&2
    exit 1
fi

if ! grep -Fq "allow_synthetic_fallback" "$HELPERS"; then
    echo "[transpile-strict-source] fallback policy flag is missing" >&2
    exit 1
fi

bad_matches="$(
    grep -RIn 'lower_program_to_mir(program, &hir, &rir)' "$TRANSPILE_DIR" \
        --include='*.cases.h' || true
)"
if [[ -n "$bad_matches" ]]; then
    echo "[transpile-strict-source] parsed-source transpile fixture uses fallback MIR:" >&2
    echo "$bad_matches" >&2
    echo "Use lower_program_to_mir_strict(...) for parsed source; keep lower_program_to_mir(...) only for synthetic AST fixtures." >&2
    exit 1
fi

parsed_fallback_matches="$(
    while IFS= read -r -d '' file; do
        awk '
            FNR == 1 { parsed_source = 0; parsed_line = 0; }
            /parse_program_from_string[[:space:]]*\(/ {
                parsed_source = 1;
                parsed_line = FNR;
            }
            /lower_program_to_mir_strict[[:space:]]*\(/ {
                if (parsed_source) {
                    parsed_source = 0;
                    parsed_line = 0;
                }
            }
            /lower_program_to_mir[[:space:]]*\(/ &&
            $0 !~ /lower_program_to_mir_strict[[:space:]]*\(/ {
                if (parsed_source) {
                    printf "%s:%d: parsed source from line %d lowers through fallback helper\n",
                           FILENAME, FNR, parsed_line;
                    parsed_source = 0;
                    parsed_line = 0;
                }
            }
        ' "$file"
    done < <(find "$TRANSPILE_DIR" -name '*.cases.h' \
        ! -name 'test_transpile_helpers.cases.h' -print0)
)"
if [[ -n "$parsed_fallback_matches" ]]; then
    echo "[transpile-strict-source] parsed-source fixture lowers through fallback MIR:" >&2
    echo "$parsed_fallback_matches" >&2
    echo "Parsed-source tests must use lower_program_to_mir_strict(...)." >&2
    exit 1
fi

synthetic_matches="$(
    grep -RInE 'lower_program_to_mir\((prog|make_program)' "$TRANSPILE_DIR" \
        --include='*.cases.h' || true
)"
if [[ -z "$synthetic_matches" ]]; then
    echo "[transpile-strict-source] synthetic fallback coverage disappeared; remove or update this gate intentionally" >&2
    exit 1
fi

echo "[transpile-strict-source] parsed-source transpile fixtures are strict; synthetic AST fallback remains explicit"

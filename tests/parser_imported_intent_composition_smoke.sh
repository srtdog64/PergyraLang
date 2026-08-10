#!/usr/bin/env bash
# Imported intent roles are finalized only after the declaration graph is whole.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy.exe}"
FIXTURE_DIR="$ROOT_DIR/tests/parser_imported_intent_composition"
BUILD_DIR="${PGY_TEST_BUILD_DIR:-$ROOT_DIR/.tmp/parser_imported_intent_composition}"
POSITIVE_AST="$BUILD_DIR/positive.ast.txt"
POSITIVE_ERR="$BUILD_DIR/positive.err.txt"
UNRESOLVED_AST="$BUILD_DIR/unresolved.ast.txt"
UNRESOLVED_ERR="$BUILD_DIR/unresolved.err.txt"
PRIVATE_AST="$BUILD_DIR/private.ast.txt"
PRIVATE_ERR="$BUILD_DIR/private.err.txt"
mkdir -p "$BUILD_DIR"

(cd "$ROOT_DIR" && "$PGY" --native-pipeline --ast \
    "tests/parser_imported_intent_composition/positive_main.pgy" \
    >"$POSITIVE_AST" 2>"$POSITIVE_ERR")

grep -Fq 'Intent: ImportedFrontendPipeline' "$POSITIVE_AST"
grep -Fq 'IntentInvolves: intake: SourceIntakeZone' "$POSITIVE_AST"
grep -Fq 'IntentValue: paths: StagePathManifest' "$POSITIVE_AST"
if grep -Fq 'IntentValue: intake: SourceIntakeZone' "$POSITIVE_AST"; then
    echo "[parser-imported-intent] zone parameter remained a value" >&2
    exit 1
fi

(cd "$ROOT_DIR" && "$PGY" --native-pipeline --ast \
    "tests/parser_imported_intent_composition/private_main.pgy" \
    >"$PRIVATE_AST" 2>"$PRIVATE_ERR")
grep -Fq 'Intent: ExportedPipeline' "$PRIVATE_AST"
grep -Eq 'IntentValue: manifest: __imp[0-9]+_LocalManifest' \
    "$PRIVATE_AST"

if (cd "$ROOT_DIR" && "$PGY" --native-pipeline --ast \
    "tests/parser_imported_intent_composition/unresolved_main.pgy" \
    >"$UNRESOLVED_AST" 2>"$UNRESOLVED_ERR"); then
    echo "[parser-imported-intent] unresolved imported parameter was accepted" >&2
    exit 1
fi
grep -Fq \
    "Intent parameter type 'MissingZone' is unresolved after declaration composition" \
    "$UNRESOLVED_ERR"
if grep -Fq 'Program:' "$UNRESOLVED_AST"; then
    echo "[parser-imported-intent] unresolved composition emitted an AST" >&2
    exit 1
fi

grep -Fq 'parser_parse_program_for_module_composition' \
    "$ROOT_DIR/src/compiler/import_resolver.c"
grep -Fq 'parser_finalize_composed_intent_parameter_roles' \
    "$ROOT_DIR/src/compiler/import_resolver.c"

echo "[parser-imported-intent] composed declaration role gate ok"

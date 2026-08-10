#!/usr/bin/env bash
# Closed fallbacks: native_builtin_capability_switch,
# selfhost_builtin_capability_switch, manifest_builtin_name_rescan,
# missing_builtin_capability_registry_success,
# native_file_mode_character_switch, selfhost_file_mode_character_switch,
# dynamic_file_mode_read_only_default, unknown_file_mode_empty_capability.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/.tmp/builtin-capability-registry"
PYTHON_BIN="${PYTHON_BIN:-python3}"
CC_BIN="${CC:-cc}"

mkdir -p "$BUILD_DIR"

"$PYTHON_BIN" "$ROOT_DIR/scripts/render_builtin_capability_registry.py" \
    "$ROOT_DIR/src/semantic/builtin_capability_registry.def" \
    "$ROOT_DIR/src/semantic/callable_contract_vocabulary.def" \
    "$ROOT_DIR/src/runtime/pgy_file_mode_capability.def" \
    "$ROOT_DIR/src/self_hosted/semantic/builtin_capability_projection_owner.pgy" \
    --check

"$CC_BIN" -std=c11 -Wall -Wextra -Werror \
    -I"$ROOT_DIR/src" \
    "$ROOT_DIR/tests/builtin_capability_registry_probe.c" \
    "$ROOT_DIR/src/semantic/capability_analyze.c" \
    "$ROOT_DIR/src/semantic/callable_contract_vocabulary.c" \
    -o "$BUILD_DIR/builtin_capability_registry_probe.exe"
"$BUILD_DIR/builtin_capability_registry_probe.exe"

if grep -R -Fq 'semantic_record_capability(ctx, PGY_CAP' \
    "$ROOT_DIR/src/semantic"; then
    echo "[builtin-capability-registry] direct native capability masks returned" >&2
    exit 1
fi
if ! grep -Fq 'capability_builtin_registry_ready()' \
    "$ROOT_DIR/src/semantic/type_checker_program.c"; then
    echo "[builtin-capability-registry] semantic admission does not fail closed" >&2
    exit 1
fi
if ! grep -Fq 'SemanticBuiltinCapabilityProjectionReady()' \
    "$ROOT_DIR/src/self_hosted/semantic/ast_capability_fact_owner.pgy"; then
    echo "[builtin-capability-registry] self-host capability facts bypass projection readiness" >&2
    exit 1
fi
if grep -Fq 'if (*p == '\''r'\'')' \
    "$ROOT_DIR/src/runtime/pgy_runtime_file_mode_capability.h"; then
    echo "[builtin-capability-registry] runtime FileOpen mode policy was reimplemented" >&2
    exit 1
fi

echo "builtin capability registry smoke: ok"

#!/usr/bin/env bash
# Rung 2 parity for the module manifest resolver (2026-05-27).
#
# Pergyra is the origin
# (src/self_hosted/tools/module_manifest_resolver/main.pgy).
# The TestHarness-projected expected JSON artifact is the clean-output oracle.
# Asserts:
#   - clean repo: rc=0, JSON byte-equal vs expected/clean.json
#   - synthetic missing-modules-key fixture: rc=1, missing_modules_key finding
# See tests/self_hosted/parity/README.md.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[self-host-parity:module-manifest-resolver] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:module-manifest-resolver] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/module_manifest_resolver}"
HARNESS_PATHS_FILE="$PERGYRA_TOOL_BUILD_DIR/module_manifest_resolver_harness_paths.txt"
mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:module-manifest-resolver" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "module-manifest-resolver-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 6 ]]; then
    echo "[self-host-parity:module-manifest-resolver] TestHarness manifest expected 6 module-manifest rows, got ${#harness_paths[@]}" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
EXPECTED_JSON_FILE="$ROOT_DIR/${harness_paths[1]}"
MANIFEST_PATH="${harness_paths[2]}"
MISSING_MODULES_FIXTURE_JSON="${harness_paths[3]}"
NESTED_MODULES_FIXTURE_JSON="${harness_paths[4]}"
NESTED_FIELD_FIXTURE_JSON="${harness_paths[5]}"

for path in "$PERGYRA_TOOL_SOURCE" "$EXPECTED_JSON_FILE" "$ROOT_DIR/$MANIFEST_PATH"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:module-manifest-resolver] missing input: $path" >&2
        exit 1
    fi
done

PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"

CLEAN_BIN="$PERGYRA_TOOL_BUILD_DIR/module_manifest_resolver_c.exe"
CLEAN_COMPILE_LOG="$PERGYRA_TOOL_BUILD_DIR/module_manifest_resolver_c.compile.log"
if ! (cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_ARG" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$CLEAN_BIN")" >"$CLEAN_COMPILE_LOG" 2>&1); then
    echo "[self-host-parity:module-manifest-resolver] C backend compile failed" >&2
    cat "$CLEAN_COMPILE_LOG" >&2
    exit 1
fi
if ! pgy_require_runnable_binary_here "self-host-parity:module-manifest-resolver" "$CLEAN_BIN"; then
    exit 1
fi

set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$CLEAN_BIN" "$MANIFEST_PATH" 2>/dev/null)"
P_RC=$?
set -e

if [[ "$P_RC" -ne 0 ]]; then
    echo "[self-host-parity:module-manifest-resolver] clean exit-code FAIL (pergyra=$P_RC)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq 'pgy.selfhost.module-manifest-resolver.v1' <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:module-manifest-resolver] schema header missing" >&2
    exit 1
fi

PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | grep -F 'pgy.selfhost.module-manifest-resolver.v1' \
    | tail -n 1)"
pgy_selfhost_compare_expected_text_artifact_with_owner \
    "self-host-parity:module-manifest-resolver" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "$EXPECTED_JSON_FILE" \
    "$PERGYRA_JSON" \
    "run_output"

# Synthetic missing-modules-key fixture.
NEG_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pgy-selfhost-mmr.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
}
trap cleanup_neg_root EXIT
mkdir -p "$NEG_ROOT/docs"
mkdir -p "$NEG_ROOT/.tmp"
printf '%s\n' "$MISSING_MODULES_FIXTURE_JSON" > "$NEG_ROOT/$MANIFEST_PATH"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$CLEAN_BIN" "$MANIFEST_PATH" 2>&1)"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:module-manifest-resolver] missing-modules-key fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"kind":"missing_modules_key"' <<<"$NEG_OUT"; then
    echo "[self-host-parity:module-manifest-resolver] missing-modules-key fixture expected missing_modules_key finding" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi

# Synthetic nested-modules fixture: a nested object may contain `modules`, but
# the document root itself does not. The JSON fact-table owner must not let a
# recursive text search satisfy the top-level modules field.
printf '%s\n' "$NESTED_MODULES_FIXTURE_JSON" > "$NEG_ROOT/$MANIFEST_PATH"

set +e
NESTED_MODULES_OUT="$(cd "$NEG_ROOT" && "$CLEAN_BIN" "$MANIFEST_PATH" 2>&1)"
NESTED_MODULES_RC=$?
set -e
if [[ "$NESTED_MODULES_RC" -ne 1 ]]; then
    echo "[self-host-parity:module-manifest-resolver] nested-modules fixture expected rc=1, got rc=$NESTED_MODULES_RC" >&2
    printf '%s\n' "$NESTED_MODULES_OUT" >&2
    exit 1
fi
if ! grep -Fq '"kind":"missing_modules_key"' <<<"$NESTED_MODULES_OUT"; then
    echo "[self-host-parity:module-manifest-resolver] nested-modules fixture expected missing_modules_key finding" >&2
    printf '%s\n' "$NESTED_MODULES_OUT" >&2
    exit 1
fi

# Synthetic nested-field fixture: a nested object may contain `layer`, but the
# module row itself does not. The JSON owner must not let recursive text search
# satisfy a top-level required-field check.
printf '%s\n' "$NESTED_FIELD_FIXTURE_JSON" > "$NEG_ROOT/$MANIFEST_PATH"

set +e
NESTED_OUT="$(cd "$NEG_ROOT" && "$CLEAN_BIN" "$MANIFEST_PATH" 2>&1)"
NESTED_RC=$?
set -e
if [[ "$NESTED_RC" -ne 1 ]]; then
    echo "[self-host-parity:module-manifest-resolver] nested-field fixture expected rc=1, got rc=$NESTED_RC" >&2
    printf '%s\n' "$NESTED_OUT" >&2
    exit 1
fi
if ! grep -Fq '"kind":"field_count_mismatch"' <<<"$NESTED_OUT" ||
   ! grep -Fq '"key":"layer"' <<<"$NESTED_OUT"; then
    echo "[self-host-parity:module-manifest-resolver] nested-field fixture expected layer field_count_mismatch" >&2
    printf '%s\n' "$NESTED_OUT" >&2
    exit 1
fi

assert_llvm_leg "self-host-parity:module-manifest-resolver" "$PERGYRA_TOOL_ARG" "$PERGYRA_TOOL_BUILD_DIR" "$MANIFEST_PATH"
echo "[self-host-parity:module-manifest-resolver] rung-2 parity ok (expected-json clean; missing-modules-key rc=1; nested-modules rc=1; nested-field rc=1)"

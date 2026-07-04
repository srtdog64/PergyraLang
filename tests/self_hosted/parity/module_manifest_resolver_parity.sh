#!/usr/bin/env bash
# Rung 2 parity for the module manifest resolver (2026-05-27).
#
# Pergyra is the origin
# (src/self_hosted/tools/module_manifest_resolver/main.pgy).
# Shell grep is the parity backend. Asserts:
#   - clean repo: rc=0, JSON byte-equal vs expected/clean.json
#   - count parity vs shell grep on docs/language_module_manifest.json
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

PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/tools/module_manifest_resolver/main.pgy"
PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/module_manifest_resolver}"
PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"
EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/module_manifest_resolver/expected/clean.json"
MANIFEST_PATH="docs/language_module_manifest.json"

for path in "$PERGYRA_TOOL_SOURCE" "$EXPECTED_JSON_FILE" "$ROOT_DIR/$MANIFEST_PATH"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:module-manifest-resolver] missing input: $path" >&2
        exit 1
    fi
done

mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"
PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL")"

# Mirror the shared lib subtree alongside the build dir so the tool's
# `import "../../lib/..."` resolves from the copied source. The tool source
# in the live tree is at `src/self_hosted/tools/<tool>/main.pgy`, and its import
# resolves to `src/self_hosted/lib/`. The build dir mirrors this two-level
# structure: tool at `.tmp/self_hosted/<tool>/main.pgy`, lib at
# `.tmp/lib/` (because `../../` from the build dir is `.tmp/`, not
# `.tmp/self_hosted/`).
LIB_BUILD_DIR="$ROOT_DIR/.tmp/lib"
mkdir -p "$LIB_BUILD_DIR"
cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy "$LIB_BUILD_DIR/"

set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_ARG" --run 2>/dev/null)"
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

# Shell drift detector.
SHELL_MODULES="$(grep -c '"name":' "$ROOT_DIR/$MANIFEST_PATH" || true)"
SHELL_BLOCKERS="$(grep -c '"beta_blocker": true' "$ROOT_DIR/$MANIFEST_PATH" || true)"
SHELL_STABLE="$(grep -c '"status": "stable-subset"' "$ROOT_DIR/$MANIFEST_PATH" || true)"
if [[ -z "$SHELL_MODULES" || "$SHELL_MODULES" -eq 0 ]]; then
    echo "[self-host-parity:module-manifest-resolver] shell ground truth empty" >&2
    exit 1
fi

if ! grep -Fq "\"modules\":${SHELL_MODULES}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:module-manifest-resolver] counts.modules parity FAIL (shell=${SHELL_MODULES})" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq "\"beta_blockers\":${SHELL_BLOCKERS}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:module-manifest-resolver] counts.beta_blockers parity FAIL (shell=${SHELL_BLOCKERS})" >&2
    exit 1
fi
if ! grep -Fq "\"stable_subset\":${SHELL_STABLE}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:module-manifest-resolver] counts.stable_subset parity FAIL (shell=${SHELL_STABLE})" >&2
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
# Strip "modules": from the manifest so the tool detects the missing key.
sed 's/"modules":/"NOTMODULES":/' "$ROOT_DIR/$MANIFEST_PATH" \
    > "$NEG_ROOT/$MANIFEST_PATH"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$PGY" "$PERGYRA_TOOL_ARG" --run 2>&1)"
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
cat > "$NEG_ROOT/$MANIFEST_PATH" <<'JSON'
{
  "schema": 1,
  "metadata": {
    "modules": []
  }
}
JSON

set +e
NESTED_MODULES_OUT="$(cd "$NEG_ROOT" && "$PGY" "$PERGYRA_TOOL_ARG" --run 2>&1)"
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
cat > "$NEG_ROOT/$MANIFEST_PATH" <<'JSON'
{
  "schema": 1,
  "modules": [
    {
      "name": "pgy.synthetic",
      "status": "stable-subset",
      "beta_blocker": true,
      "surfaces": [
        { "layer": "not-a-module-layer" }
      ]
    }
  ]
}
JSON

set +e
NESTED_OUT="$(cd "$NEG_ROOT" && "$PGY" "$PERGYRA_TOOL_ARG" --run 2>&1)"
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

assert_llvm_leg "self-host-parity:module-manifest-resolver" "$PERGYRA_TOOL_ARG" "$PERGYRA_TOOL_BUILD_DIR"
echo "[self-host-parity:module-manifest-resolver] rung-2 parity ok (modules=$SHELL_MODULES blockers=$SHELL_BLOCKERS stable=$SHELL_STABLE; missing-modules-key rc=1; nested-modules rc=1; nested-field rc=1)"

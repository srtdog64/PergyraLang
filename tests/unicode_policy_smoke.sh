#!/usr/bin/env bash
set -euo pipefail

# Subject of this gate:
#   the unicode policy changed.
# That is a fact about the native pipeline, so the gate compiles
# in-process instead of delegating to the installed self-host driver.
# Delegated, a self-host coverage gap would read as a regression in
# the subject above. Declared per harness because the compiler is
# reached through make and nested scripts, and the variable is the
# same declared opt-out as --native-pipeline -- never a fallback.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY_WAS_EXPLICIT=0
if [[ -n "${PGY_BIN:-}" ]]; then
    PGY_WAS_EXPLICIT=1
fi
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"

POLICY_DOC="$ROOT_DIR/docs/110_string_unicode_policy.md"
if [[ ! -f "$POLICY_DOC" ]]; then
    echo "[unicode-policy] missing unicode policy doc: $POLICY_DOC" >&2
    exit 1
fi
for required in \
    "String And Unicode Beta Policy" \
    "beta-freeze-source-of-truth" \
    "UTF-8 string payloads" \
    "byte-length for beta" \
    "byte-exact and normalization-blind" \
    "Unicode identifiers are not beta-stable" \
    "Locale-sensitive comparison" \
    "Grapheme-cluster iteration" \
    "make unicode-policy-test-smoke"; do
    if ! grep -Fq "$required" "$POLICY_DOC"; then
        echo "[unicode-policy] policy doc missing: $required" >&2
        exit 1
    fi
done

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_WAS_EXPLICIT" -eq 1 ]]; then
        echo "[unicode-policy] missing compiler binary: $PGY" >&2
        exit 1
    fi
    echo "[unicode-policy] SKIP executable probe; source policy is gated"
    exit 0
fi

WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_unicode_policy.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

cat > "$WORK_DIR/utf8_strings.pgy" <<'EOF'
func Main() -> Void {
    let greeting: String = "안녕, Pergyra";
    Log(greeting);
    Log(Contains(greeting, "녕"));
    Log(Replace(greeting, "Pergyra", "세계"));
    Log(StringLength("é"));
}
EOF

cat > "$WORK_DIR/unicode_identifier_reject.pgy" <<'EOF'
func Main() -> Void {
    let 값: Int = 1;
    Log(값);
}
EOF

run_backend() {
    local backend="$1"
    local output
    local source_arg

    source_arg="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/utf8_strings.pgy")"
    output="$("$PGY" "$source_arg" --backend="$backend" --run 2>&1)"
    for expected in "안녕, Pergyra" "true" "안녕, 세계" "2"; do
        if ! grep -Fq "$expected" <<<"$output"; then
            echo "[unicode-policy] backend=$backend missing '$expected'" >&2
            echo "--- output ---" >&2
            echo "$output" >&2
            echo "--------------" >&2
            exit 1
        fi
    done
    echo "[unicode-policy] backend=$backend ok"
}

BACKENDS="${PGY_UNICODE_BACKENDS:-c llvm}"
for backend in $BACKENDS; do
    run_backend "$backend"
done

if "$PGY" "$(pgy_path_for_compiler "$PGY" "$WORK_DIR/unicode_identifier_reject.pgy")" --backend=c >/dev/null 2>"$WORK_DIR/reject.err"; then
    echo "[unicode-policy] unicode identifier unexpectedly accepted" >&2
    exit 1
fi
if ! grep -Eiq "invalid|unexpected|syntax|identifier|token|parse" "$WORK_DIR/reject.err"; then
    echo "[unicode-policy] unicode identifier rejection did not look diagnostic-like" >&2
    cat "$WORK_DIR/reject.err" >&2
    exit 1
fi

echo "[unicode-policy] string/unicode beta policy ok"

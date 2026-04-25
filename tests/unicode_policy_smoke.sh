#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"

if [[ ! -x "$PGY" ]]; then
    echo "[unicode-policy] missing compiler binary: $PGY" >&2
    exit 1
fi

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

    output="$("$PGY" "$WORK_DIR/utf8_strings.pgy" --backend="$backend" --run 2>&1)"
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

if "$PGY" "$WORK_DIR/unicode_identifier_reject.pgy" --backend=c >/dev/null 2>"$WORK_DIR/reject.err"; then
    echo "[unicode-policy] unicode identifier unexpectedly accepted" >&2
    exit 1
fi
if ! grep -Eiq "invalid|unexpected|syntax|identifier|token|parse" "$WORK_DIR/reject.err"; then
    echo "[unicode-policy] unicode identifier rejection did not look diagnostic-like" >&2
    cat "$WORK_DIR/reject.err" >&2
    exit 1
fi

echo "[unicode-policy] string/unicode beta policy ok"

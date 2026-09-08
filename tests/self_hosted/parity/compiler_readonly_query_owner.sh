#!/usr/bin/env bash
# Focused execution of reached production queries; not whole-driver closure.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL=self-host-compiler-readonly-query
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
fail() { echo "[$LABEL] $*" >&2; exit 1; }
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/compiler-readonly-query.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
SOURCE=tests/self_hosted/fixtures/compiler_readonly_query.pgy
echo "[$LABEL] evidence: $REL"
printf '0\n2\n1\nfalse\ntrue\n0\n1\n-1\n1\n-1\ntrue\nfalse\n0\n-1\n1\n-1\nfalse\nfalse\n-1\n3\n29\nsecond\n1\n' >"$WORK/expected.run"
for origin in native public; do
    for backend in c llvm; do
        stem="$origin-$backend"
        command=("$PGY" "$SOURCE")
        [[ "$origin" == native ]] && command+=(--native-pipeline)
        command+=("--backend=$backend" --opt=dev -o "$REL/$stem.exe")
        (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE PGY_DEBUG_PIPELINE_TIMING=1 \
            PGY_SELF_DRIVER_BIN="$DRIVER" "${command[@]}") >"$WORK/$stem.compile.log" 2>&1 || {
            cat "$WORK/$stem.compile.log" >&2; fail "$stem compilation failed";
        }
        if [[ "$origin" == public ]]; then
            ! grep -Fq '[pipeline timing]' "$WORK/$stem.compile.log" || fail "$stem used native fallback"
        fi
        timeout 30s "$WORK/$stem.exe" >"$WORK/$stem.raw" 2>"$WORK/$stem.err" || {
            cat "$WORK/$stem.err" >&2; fail "$stem query execution failed";
        }
        tr -d '\r' <"$WORK/$stem.raw" >"$WORK/$stem.run"
        cmp -s "$WORK/expected.run" "$WORK/$stem.run" || {
            diff -u "$WORK/expected.run" "$WORK/$stem.run" >&2; fail "$stem query results differed";
        }
    done
done
echo "[$LABEL] native/public C/LLVM: 23 query, range, duplicate and input-preservation results PASS"

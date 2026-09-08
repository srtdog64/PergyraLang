#!/usr/bin/env bash
# Exact call-view projection; the full-source bootstrap remains a separate gate.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL=self-host-call-spine-roots
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
fail() { echo "[$LABEL] $*" >&2; exit 1; }
VERDICT="$ROOT_DIR/src/self_hosted/semantic/ast_named_value_boundary_verdict_owner.pgy"
if grep -Fq 'SemanticCallSpineViewForCallNodeWithin' "$VERDICT"; then
    fail 'named boundary reopened the per-call arena scan'
fi
[[ "$(grep -Fc 'SemanticCallSpineRootsFromGraph(graph)' "$VERDICT")" == 1 ]] || fail 'named boundary must project once'
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/call-spine-roots.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
SOURCE=tests/self_hosted/fixtures/call_spine_root_projection.pgy
echo "[$LABEL] evidence: $REL"
sha256sum "$PGY" "$DRIVER" "$ROOT_DIR/$SOURCE" "$VERDICT" \
    "$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_call_view_owner.pgy" >"$WORK/input.sha256"
printf 'true\ntrue\ntrue\n21\n9\n12\n17\nInt\nString\n9\n12\n18\nfalse\nfalse\nfalse\n' >"$WORK/expected.run"
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
            cat "$WORK/$stem.err" >&2; fail "$stem projection execution failed";
        }
        tr -d '\r' <"$WORK/$stem.raw" >"$WORK/$stem.run"
        cmp -s "$WORK/expected.run" "$WORK/$stem.run" || {
            diff -u "$WORK/expected.run" "$WORK/$stem.run" >&2; fail "$stem projection differed";
        }
        echo "[$LABEL] $stem: exact views and missing-projection controls PASS"
    done
done
"${PYTHON_BIN:-python}" "$ROOT_DIR/tests/self_hosted/parity/call_spine_root_projection_pressure.py" "$WORK/native-c.exe"
echo "[$LABEL] native/public C/LLVM projection parity: PASS"

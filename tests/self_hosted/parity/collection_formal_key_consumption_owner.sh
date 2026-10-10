#!/usr/bin/env bash
# Behavioral facts are owned by native C/LLVM execution. The separate static
# ratchet checks only key placement; it certifies neither semantics nor cost.
set -Eeuo pipefail
trap 'status=$?; echo "[collection-formal-key] failed at line $LINENO (status $status); evidence: ${REL:-not-created}" >&2' ERR
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
cd "$ROOT_DIR"
MODE="${1:-all}"
case "$MODE" in all|--static-only|--behavior-only) ;; *) echo 'unknown formal-key gate mode' >&2; exit 2 ;; esac
if [[ "$MODE" != --behavior-only ]]; then
    python3 - src/self_hosted/semantic/ast_collection_formal_use_owner.pgy <<'PY'
import pathlib, re, sys

def key_consumption_admitted(source):
    # Lexical source ratchet only. Strip comments/strings while preserving
    # offsets; then check the two unique conversions are inside their consumer
    # guards. Execution below independently pins effects and refusals.
    source = re.sub(r'//[^\n]*|"(?:\\.|[^"\\])*"', lambda m: ' ' * len(m[0]), source)
    start = source.index('func SemanticAstCollectionFormalEffectsWithSurfaceOrder(')
    source = source[start:]
    def close_at(opening):
        depth = 1
        for pos in range(opening + 1, len(source)):
            depth += (source[pos] == '{') - (source[pos] == '}')
            if depth == 0:
                return pos
        raise AssertionError('unbalanced owner scope')
    opening = source.index('{')
    source = source[:close_at(opening) + 1]
    checks = [
        (r'if\s+binding\.kind\s*==\s*SemanticExpressionBindingFormalParameter\(\)\s*\{',
         r'ToString\(\s*binding\.syntax_id\s*\)'),
        (r'if\s+ok\s*&&\s*\(\s*node_rows\[root\]\s*>=\s*0\s*\|\|\s*element_rows\[root\]\s*>=\s*0\s*\)\s*\{',
         r'ToString\(\s*root\s*\)'),
    ]
    for guard, conversion in checks:
        positions = list(re.finditer(conversion, source))
        guards = list(re.finditer(guard, source))
        if len(positions) != 1 or len(guards) != 1:
            return False
        beginning = guards[0].end() - 1
        if not beginning < positions[0].start() < close_at(beginning):
            return False
    return True

# Negative controls prove this structural check rejects each historical hoist,
# independently of whether the checked-out producer has been optimized yet.
formal = 'if binding.kind == SemanticExpressionBindingFormalParameter() { '
root = 'if ok && (node_rows[root] >= 0 || element_rows[root] >= 0) { '
prefix = 'func SemanticAstCollectionFormalEffectsWithSurfaceOrder() { '
good = prefix + formal + 'let key: String = ToString(binding.syntax_id); } ' + root + 'let root_key: String = ToString(root); } }'
assert key_consumption_admitted(good)
assert not key_consumption_admitted(prefix + 'let key: String = ToString(binding.syntax_id); ' + formal + '} ' + root + 'let root_key: String = ToString(root); } }')
assert not key_consumption_admitted(prefix + formal + 'let key: String = ToString(binding.syntax_id); } let root_key: String = ToString(root); ' + root + '} }')
if not key_consumption_admitted(pathlib.Path(sys.argv[1]).read_text()):
    raise SystemExit('unconsumed formal/root key formatting returned (structural placement ratchet only)')
print('[collection-formal-key] guarded key placement and two planted hoist refusals PASS (static only)')
PY
fi
[[ "$MODE" != --static-only ]] || exit 0
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here collection-formal-key "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/collection-formal-key.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
PROBE=tests/self_hosted/fixtures/collection_formal_key_consumption_probe.pgy
FIXTURES=tests/self_hosted/parity/fixture/collection_field_lifetime
INPUTS=("$FIXTURES/field_ctor_own_formal_unescaped_statements_positive.pgy"
    "$FIXTURES/inout_index_formal_shallow_positive.pgy" "$FIXTURES/borrow_formal_identity_input.pgy"
    "$FIXTURES/own_formal_read_after_forward_negative.pgy" "$FIXTURES/borrow_formal_ref_own_negative.pgy"
    "$FIXTURES/borrow_formal_fresh_deferred_negative.pgy"
    "$FIXTURES/field_ctor_own_formal_deferred_opaque_mutation_negative.pgy"
    "$FIXTURES/inout_index_formal_unknown_negative.pgy"
    tests/self_hosted/parity/fixture/collection_member_place/text_formal_reader_element_positive.pgy
    "$FIXTURES/inout_forward_owned_push_drop_positive.pgy")
sha256sum "$PGY" >"$WORK/native.sha256"
sha256sum "$PROBE" "${BASH_SOURCE[0]}" >"$WORK/probe-source.sha256"
for input in "${INPUTS[@]}"; do sha256sum "$input"; done >"$WORK/inputs.sha256"
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z | xargs -0 sha256sum >"$WORK/imports.sha256"
echo "[collection-formal-key] compiler=$PGY evidence=$REL"
cat "$WORK/native.sha256"
# Exact pre-edit semantic controls. Indexed assignment is deliberately effect5;
# it is NOT the ArraySet parser-statement Atom-target exception.
printf 'body=ok\nAppend:0:2=6\nPop:0:2=6\nSet:0:2=6\nIndexed:0:2=5\ntargets=3:0\nordinary=5\n' >"$WORK/expected-0"
printf 'body=ok\nMatches:0:0=3\nAppendBorrowed:0:1=6\nForward:0:1=6\ntargets=1:0\nordinary=6\n' >"$WORK/expected-1"
printf 'body=ok\nMetadata:0:2=1\nMatches:0:0=3\nForeign:0:3=1\nModes:0:0=-1\nModes:1:1=-1\nModes:2:2=-1\nModes:3:3=-1\ntargets=0:0\nordinary=12\n' >"$WORK/expected-2"
printf 'body=move_from_released\nRetire:0:2=1\nForward:0:2=1\ntargets=0:0\nordinary=6\n' >"$WORK/expected-3"
printf 'body=borrow_boundary_escape\nMetadata:0:2=1\nObserve:0:3=-1\ntargets=0:0\nordinary=4\n' >"$WORK/expected-4"
printf 'body=borrow_boundary_escape\nMetadata:0:2=1\nOpaque:0:1=6\nObserve:0:3=4\ntargets=1:0\nordinary=8\n' >"$WORK/expected-5"
printf 'body=borrow_boundary_escape\nOpaque:0:1=6\nStore:0:2=4\ntargets=1:0\nordinary=3\n' >"$WORK/expected-6"
printf 'body=borrow_boundary_escape\nMatches:0:0=3\nOpaque:0:1=4\nForward:0:1=4\ntargets=1:0\nordinary=6\n' >"$WORK/expected-7"
printf 'body=ok\nHas:0:3=3\nCheck:0:3=3\ntargets=0:0\nordinary=26\n' >"$WORK/expected-8"
printf 'body=ok\nPopulate:0:1=2\nForward:0:1=2\ntargets=0:0\nordinary=5\n' >"$WORK/expected-9"
printf 'body=ok\nEscape:0:3=5\ntargets=0:0\nordinary=1\n' >"$WORK/expected-element-escape"
printf 'MALFORMED FORMAL KEY FACTS REFUSED\n' >"$WORK/expected-malformed"
for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline "$PROBE" "--backend=$backend" --opt=dev \
        -o "$REL/$backend-key.exe" >"$WORK/$backend.compile" 2>&1
    sha256sum "$WORK/$backend-key.exe" >>"$WORK/probe-binaries.sha256"
    for index in "${!INPUTS[@]}"; do
        timeout 30 "$WORK/$backend-key.exe" "${INPUTS[$index]}" facts \
            >"$WORK/$backend-$index.raw" 2>"$WORK/$backend-$index.err"
        tr -d '\r' <"$WORK/$backend-$index.raw" >"$WORK/$backend-$index.run"
        grep '^carrier:' "$WORK/$backend-$index.run" | LC_ALL=C sort >"$WORK/$backend-$index.carrier"
        grep -v '^carrier:' "$WORK/$backend-$index.run" >"$WORK/$backend-$index.semantic"
        [[ ! -s "$WORK/$backend-$index.err" ]]
        cmp "$WORK/expected-$index" "$WORK/$backend-$index.semantic"
        if [[ "$index" == 8 ]]; then
            for field in size=3 20=1 36=1 60=1; do
                grep -Fxq "carrier:map:borrowed-text:$field" "$WORK/$backend-$index.carrier"
            done
        elif [[ "$index" == 9 ]]; then
            grep -Fxq 'carrier:map:owned-push:size=1' "$WORK/$backend-$index.carrier"
            grep -Fxq 'carrier:map:owned-push:3=1' "$WORK/$backend-$index.carrier"
        fi
    done
    # Admitted borrowed-element statement-target positive is OPEN: indexed
    # String mutator sources refuse before this owner. Do not forge typed AST
    # to count coverage. The real borrowed-root escape has no target exemption.
    for embedded in element-escape; do
        timeout 30 "$WORK/$backend-key.exe" "embedded-$embedded" facts \
            >"$WORK/$backend-$embedded.raw" 2>"$WORK/$backend-$embedded.err"
        tr -d '\r' <"$WORK/$backend-$embedded.raw" >"$WORK/$backend-$embedded.run"
        grep '^carrier:' "$WORK/$backend-$embedded.run" | LC_ALL=C sort >"$WORK/$backend-$embedded.carrier"
        grep -v '^carrier:' "$WORK/$backend-$embedded.run" >"$WORK/$backend-$embedded.semantic"
        [[ ! -s "$WORK/$backend-$embedded.err" ]]
        cmp "$WORK/expected-$embedded" "$WORK/$backend-$embedded.semantic"
    done
    for mutation in formal-binding ordinary-binding order-unready order-truncated; do
        timeout 30 "$WORK/$backend-key.exe" "$FIXTURES/borrow_formal_identity_input.pgy" "$mutation" \
            >"$WORK/$backend-$mutation.raw" 2>"$WORK/$backend-$mutation.err"
        tr -d '\r' <"$WORK/$backend-$mutation.raw" >"$WORK/$backend-$mutation.run"
        grep '^carrier:' "$WORK/$backend-$mutation.run" | LC_ALL=C sort >"$WORK/$backend-$mutation.carrier"
        grep -v '^carrier:' "$WORK/$backend-$mutation.run" >"$WORK/$backend-$mutation.semantic"
        [[ ! -s "$WORK/$backend-$mutation.err" ]]
        cmp "$WORK/expected-malformed" "$WORK/$backend-$mutation.semantic"
    done
    for mutation in garbage facts-extra; do
        if "$WORK/$backend-key.exe" "$FIXTURES/borrow_formal_identity_input.pgy" "$mutation" \
            >"$WORK/$backend-$mutation.raw" 2>"$WORK/$backend-$mutation.err"; then
            echo 'observer accepted unknown mode' >&2; exit 1
        else status=$?; fi
        [[ "$status" != 0 && ! -s "$WORK/$backend-$mutation.err" ]]
        grep -Fxq 'unknown formal-key mode' <(tr -d '\r' <"$WORK/$backend-$mutation.raw")
    done
done
for case_name in "${!INPUTS[@]}" element-escape formal-binding ordinary-binding order-unready order-truncated; do
    cmp "$WORK/c-$case_name.carrier" "$WORK/llvm-$case_name.carrier"
    sha256sum "$WORK/c-$case_name.carrier" >>"$WORK/carrier-baseline.sha256"
done
for manifest in native probe-source inputs imports probe-binaries; do
    sha256sum --quiet -c "$WORK/$manifest.sha256"
done
echo "[collection-formal-key] native C/LLVM pinned effects/verdicts, formal-target/ordinary/borrowed-root controls and malformed fact refusals PASS; evidence=$REL; admitted element-target/whole-P1/installed-driver/cost OPEN"

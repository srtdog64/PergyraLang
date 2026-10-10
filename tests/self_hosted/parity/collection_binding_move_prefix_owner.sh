#!/usr/bin/env bash
# Behavioral source-to-facts probe; no source inventory or timing is a verdict.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$ROOT_DIR"
MODE="${1:-all}"
case "$MODE" in all|--static-only|--behavior-only) ;; *) echo 'unknown binding-move gate mode' >&2; exit 2 ;; esac
if [[ "$MODE" != --behavior-only ]]; then
    python3 - src/self_hosted/semantic/ast_collection_ownership_binding_move_use_owner.pgy <<'PY'
import pathlib, re, sys

def prefix_placement_admitted(source):
    # This is a shrink-only lexical placement ratchet, not a semantic or cost
    # certificate. Preserve offsets while removing quoted text and comments.
    source = re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"',
                    lambda m: ' ' * len(m[0]), source)
    owner = 'func SemanticAstCollectionFirstInvalidBindingMoveUse('
    if source.count(owner) != 1:
        return False
    source = source[source.index(owner):]
    def close_at(opening):
        depth = 1
        for pos in range(opening + 1, len(source)):
            depth += (source[pos] == '{') - (source[pos] == '}')
            if depth == 0:
                return pos
        raise ValueError('unbalanced owner scope')
    def unique(pattern):
        matches = list(re.finditer(pattern, source))
        if len(matches) != 1:
            raise ValueError('missing or repeated placement anchor')
        return matches[0]
    def scope(pattern):
        match = unique(pattern)
        opening = match.end() - 1
        return opening, close_at(opening)
    def inside(position, boundary):
        return boundary[0] < position < boundary[1]
    try:
        opening = source.index('{')
        source = source[:close_at(opening) + 1]
        roots = scope(r'while\s+root_slot\s*<\s*ArrayLength\(\s*surfaces\.expression_graph\.roots\s*\)\s*\{')
        present = scope(r'if\s+surfaces\.expression_graph\.has_roots\[\s*root_slot\s*\]\s*\{')
        guarded = scope(r'if\s+MapSize\(\s*move_points\s*\)\s*>\s*0\s*\{')
        nodes = scope(r'while\s+node\s*<=\s*root_id\s*\{')
        completed = scope(r'if\s+completed\s*>=\s*0\s*\{')
        if not (inside(present[0], roots) and inside(present[1], roots) and
                inside(guarded[0], present) and inside(guarded[1], present) and
                inside(nodes[0], guarded) and inside(nodes[1], guarded)):
            return False
        for pattern in (
            r'SemanticExpressionGraphSubtreeStart\s*\(',
            r'SemanticAstScopedLocalBindingIdentityForGraphLeaf\s*\(',
            r'ToString\(\s*definitions\.syntax_ids\[\s*current\[\s*binding\.row\s*\]\s*\]\s*\)',
            r'MapHas\(\s*move_points\s*,\s*key\s*\)',
        ):
            if not inside(unique(pattern).start(), guarded):
                return False
        completion = unique(r'SemanticAstCollectionDefinitionCompletedAtSlot\s*\(').start()
        insertion = unique(r'MapSet\(\s*move_points\s*,').start()
        update = unique(r'current\[\s*definitions\.binding_rows\[\s*completed\s*\]\s*\]\s*=\s*completed\s*;').start()
        # Completion belongs to the SAME has_roots boundary, AFTER the read
        # guard; neither retirement insertion nor lineage update may move in.
        return (guarded[1] < completion < completed[0] < insertion < update < completed[1] < present[1] and
                inside(completion, present) and inside(insertion, completed) and
                inside(update, completed))
    except ValueError:
        return False

prefix = 'func SemanticAstCollectionFirstInvalidBindingMoveUse() { while root_slot < ArrayLength(surfaces.expression_graph.roots) { if surfaces.expression_graph.has_roots[root_slot] { '
read = 'let start = SemanticExpressionGraphSubtreeStart(surfaces.expression_graph, root_id); while node <= root_id { let binding = SemanticAstScopedLocalBindingIdentityForGraphLeaf(); let key = ToString(definitions.syntax_ids[current[binding.row]]); if MapHas(move_points, key) {} } '
completion = 'let completed = SemanticAstCollectionDefinitionCompletedAtSlot(); if completed >= 0 { MapSet(move_points, key, syntax_id); current[definitions.binding_rows[completed]] = completed; } '
guard = 'if MapSize(move_points) > 0 { '
suffix = '} } }'
good = prefix + guard + read + '} ' + completion + suffix
assert prefix_placement_admitted(good)
assert not prefix_placement_admitted(prefix + read + completion + suffix), 'historical unconditional read admitted'
assert not prefix_placement_admitted(prefix + guard + read + completion + '} ' + suffix), 'completion hidden in read guard admitted'
assert not prefix_placement_admitted(prefix + 'if true { ' + read + '} ' + completion + suffix), 'always-true guard admitted'
assert not prefix_placement_admitted(good.replace('MapSize(move_points) > 0', 'MapSize(move_points) >= 0')), 'non-restricting size guard admitted'
if not prefix_placement_admitted(pathlib.Path(sys.argv[1]).read_text()):
    raise SystemExit('binding-move read/initial-definition transition placement refused (structural only)')
print('[binding-move-prefix] guarded read placement and four planted refusals PASS (static only)')
PY
fi
[[ "$MODE" != --static-only ]] || exit 0
source tests/pgy_binary_path_helpers.sh
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here collection-binding-move-prefix "$PGY"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/collection-binding-move.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
PROBE=tests/self_hosted/fixtures/collection_binding_move_prefix_probe.pgy
CASES=(no-moves late-move borrowed-handoff borrowed-exclusive pre-reassign post-reassign
    shadowing shadow-rejected lhs-rhs self-assignment first-invalid-root first-invalid-node
    first-invalid-node-reversed member-failure formal-failure type-failure)
# Fixed admitted source corpus above lives in the probe. These pre-edit stdout
# hashes pin the FULL normalized observation, not only a selected diagnostic:
# scanner triple/text, caller/body message and node IDs, canonical rows/storage
# lineage and every root's completion/start observation. The two rejected-body
# inputs are NOT_OBSERVED, not evidence of production scanner call order.
EXPECTED=(
    5b8bd35b336ef04eb45d13692b2d7895243e0e157c61492fc7480600aa5dcea7
    3605478c9d38eb0db1f5f0b2da21a1bc8b63e265a9c396582f3765fe80c0f798
    fb0e6ecb004e07765e3133e7cad6c28b08be4828ae9d2d1026ff80c6ce2f6226
    a1ebfbb37bbb0b4c2fe186bfc540e6a7be992644e44c716d6f13453607f41e4e
    5411727cbd382ef69ec118c910996cdd8ce2c6f93013e1ba33fbc822eba803b9
    e2c0caa354ea93d27c911f4884b5b072c0082912aa948a430549f73bc4781543
    03dd50d0787511afee019c195ded889a3a77b9c13f828391d2f4be0d64eb900b
    44f7d6d98ffef1fe21a042aef784a008a599d90ebe33ae32e3a4d3a4f69a9f22
    695235f3ed85efbaaf8b81d87c67ec42934c75efe9e77b0e5a42d94c33e40a54
    f57501c2a389117956dcf836e09b08b86d2afae2d4b10df5db45b8da19e254ab
    62e18f9bbb38e5dcdbc04e516b684d7ea62bf33064210d02fd9d14a9bcf7eb71
    313ccb06ca98b4580a17a391343cde2631beffce1d933605f10373422fea5ab8
    ee4068c374bb9fd01b2b971b6673b7f110415fd629655fd6ceb742d1771c1fbe
    1ede8e838159039090842cba97b3366420c4e35f73bc3c27b73bf31001ae53c3
    9079aaa3386ea2bbac672560b740eb3d4e2ca2abc87cee1bd501a6a4db922d84
    3257a64fb5fe7f5f45883dfc1959fcef80eef4c5b2baac07ae9f1de4d5a694ce
    708b1d69d9736786b60330880e1d34f5b503ac82805b6c013d4e9b19509676c7
)
sha256sum "$PGY" >"$WORK/native.sha256"
sha256sum "$PROBE" "${BASH_SOURCE[0]}" >"$WORK/probe-source.sha256"
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z | xargs -0 sha256sum >"$WORK/imports.sha256"
echo "[binding-move-prefix] compiler=$PGY evidence=$REL"
cat "$WORK/native.sha256"
for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline "$PROBE" "--backend=$backend" --opt=dev \
        -o "$REL/$backend-binding.exe" >"$WORK/$backend.compile" 2>&1
    sha256sum "$WORK/$backend-binding.exe" >>"$WORK/probe-binaries.sha256"
    for name in "${CASES[@]}" malformed-analysis; do
        input="$name"; mode=facts
        if [[ "$name" == malformed-analysis ]]; then input=no-moves; mode=malformed-analysis; fi
        timeout 15 "$WORK/$backend-binding.exe" "$input" "$mode" >"$WORK/$backend-$name.raw" 2>"$WORK/$backend-$name.err"
        [[ ! -s "$WORK/$backend-$name.err" ]]
        tr -d '\r' <"$WORK/$backend-$name.raw" >"$WORK/$backend-$name.run"
    done
done
OBSERVED=("${CASES[@]}" malformed-analysis)
[[ "${#OBSERVED[@]}" == "${#EXPECTED[@]}" ]]
for index in "${!OBSERVED[@]}"; do
    name="${OBSERVED[$index]}"
    cmp "$WORK/c-$name.run" "$WORK/llvm-$name.run"
    printf '%s  %s\n' "${EXPECTED[$index]}" "$WORK/c-$name.run" | sha256sum --quiet -c -
    sha256sum "$WORK/c-$name.run" >>"$WORK/observations.sha256"
    if [[ -n "${BINDING_MOVE_BASELINE:-}" ]]; then cmp "$BINDING_MOVE_BASELINE/c-$name.run" "$WORK/c-$name.run"; fi
done
for manifest in native probe-source imports probe-binaries; do sha256sum --quiet -c "$WORK/$manifest.sha256"; done
echo "[binding-move-prefix] C/LLVM pinned full scanner/caller/definition/storage observations PASS; evidence=$REL; missing-subtree admitted positive/strict same-SyntaxId later-lane positive/whole-P1/installed-driver/cost OPEN"

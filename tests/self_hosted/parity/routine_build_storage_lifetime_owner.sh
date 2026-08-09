#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PYTHON="${PYTHON:-python}"
BUILD_DIR="${PGY_ROUTINE_BUILD_STORAGE_TEST_DIR:-$ROOT_DIR/.tmp/self_hosted/routine_build_storage_lifetime}"
OWNER="$ROOT_DIR/src/self_hosted/mir/routine_build_storage_lifetime_owner.pgy"
MATCH_OWNER="$ROOT_DIR/src/self_hosted/mir/routine_match_owner.pgy"
ARTIFACT_OWNER="$ROOT_DIR/src/self_hosted/mir/artifact_lower_owner.pgy"
SOURCE_MIR_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy"
AST_LIFETIME_OWNER="$ROOT_DIR/src/self_hosted/mir/ast_arena_storage_lifetime_owner.pgy"
NEGATIVE="$ROOT_DIR/tests/self_hosted/semantic/fixture/compiler_retire_array_storage_external_rejected.pgy"
IMPERSONATION_NEGATIVE="$ROOT_DIR/tests/self_hosted/semantic/fixture/compiler_retire_array_storage_owner_impersonation_rejected.pgy"

if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[routine-build-storage-lifetime] missing compiler: $PGY" >&2
    exit 1
fi
command -v "$PYTHON" >/dev/null 2>&1 || {
    echo "[routine-build-storage-lifetime] missing python: $PYTHON" >&2
    exit 1
}
mkdir -p "$BUILD_DIR"

"$PYTHON" - "$ROOT_DIR" <<'PY'
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])
owner_path = root / "src/self_hosted/mir/routine_build_storage_lifetime_owner.pgy"
artifact_path = root / "src/self_hosted/mir/artifact_lower_owner.pgy"
routine_build_path = root / "src/self_hosted/mir/routine_build_owner.pgy"
source_mir_path = root / "src/self_hosted/compiler/driver_rung2_owner.pgy"
ast_lifetime_path = root / "src/self_hosted/mir/ast_arena_storage_lifetime_owner.pgy"

structs = {}
struct_pattern = re.compile(r"(?ms)^struct\s+(\w+)\s*\{(.*?)^\}")
field_pattern = re.compile(r"(?m)^\s*(\w+)\s*:\s*([^;]+);")
for path in (root / "src/self_hosted").rglob("*.pgy"):
    text = path.read_text(encoding="utf-8")
    for name, body in struct_pattern.findall(text):
        fields = field_pattern.findall(body)
        structs.setdefault(name, []).append(fields)

def leaves(type_name, prefix, stack):
    direct = re.fullmatch(r"Array\s*<\s*([^>]+)\s*>", type_name.strip())
    if direct:
        element = direct.group(1).strip()
        if element not in {"Int", "String"}:
            raise SystemExit(f"unsupported routine-build Array leaf {prefix}: {element}")
        return [(prefix, element)]
    name = type_name.strip()
    if name not in structs:
        return []
    if len(structs[name]) != 1:
        raise SystemExit(f"ambiguous routine-build struct owner for {name}")
    if name in stack:
        raise SystemExit(f"recursive routine-build carrier at {prefix}: {name}")
    result = []
    for field, field_type in structs[name][0]:
        result.extend(leaves(field_type, f"{prefix}.{field}", stack | {name}))
    return result

expected = leaves("SelfMirRoutineBuild", "build", set())
owner = owner_path.read_text(encoding="utf-8")
binding_pattern = re.compile(
    r"let\s+(\w+)\s*:\s*Array\s*<\s*(Int|String)\s*>\s*=\s*"
    r"(build(?:\.\w+)+)\s*;"
)
bindings = binding_pattern.findall(owner)
actual = [(path, element) for _, element, path in bindings]
if sorted(expected) != sorted(actual):
    missing = sorted(set(expected) - set(actual))
    extra = sorted(set(actual) - set(expected))
    raise SystemExit(f"routine-build leaf coverage drift: missing={missing} extra={extra}")
if len(actual) != len(set(actual)):
    raise SystemExit("routine-build leaf binding is duplicated")

locals_by_path = {path: local for local, _, path in bindings}
drops = re.findall(r"CompilerRetireArrayStorage\((\w+)\)\s*;", owner)
expected_locals = sorted(locals_by_path.values())
if sorted(drops) != expected_locals or len(drops) != len(set(drops)):
    raise SystemExit("routine-build backing drop coverage is not exactly once per leaf")

int_count = sum(element == "Int" for _, element in actual)
string_count = sum(element == "String" for _, element in actual)
if (int_count, string_count, len(actual)) != (41, 43, 84):
    raise SystemExit(
        f"routine-build leaf census drift: Int={int_count} String={string_count} total={len(actual)}"
    )

if "ArrayDropOwnedStrings" in owner:
    raise SystemExit("routine-build lifetime owner must not free shared String elements")
for forbidden in (r"(?<!\w)facts\.", r"(?<!\w)analysis\.",
                  r"(?<!\w)input\.", r"(?<!\w)expression_graph\b"):
    if re.search(forbidden, owner):
        raise SystemExit(f"routine-build lifetime owner reaches forbidden target: {forbidden}")

artifact = artifact_path.read_text(encoding="utf-8")
normal_append = artifact.index("facts = SelfMirAppendRoutine(")
normal_retire = artifact.index(
    "SelfMirRoutineBuildStorageRetireAfterLastConsumer(build);", normal_append
)
normal_done = artifact.index('observe_pressure, "routine", i, "done"')
intent_append = artifact.index("facts = SelfMirAppendIntentRoutine(")
intent_retire = artifact.index(
    "SelfMirRoutineBuildStorageRetireAfterLastConsumer(intent_build);", intent_append
)
intent_done = artifact.index('observe_pressure, "intent", intent_i, "done"')
if not (normal_append < normal_retire < normal_done):
    raise SystemExit("normal routine storage retirement is not append < retire < done")
if not (intent_append < intent_retire < intent_done):
    raise SystemExit("intent routine storage retirement is not append < retire < done")
if artifact.count("SelfMirRoutineBuildStorageRetireAfterLastConsumer(") != 4:
    raise SystemExit("routine-build retirement must cover success+failure for normal+intent")

ast_lifetime = ast_lifetime_path.read_text(encoding="utf-8")
non_traversal_fields = [
    "nominal_subkinds", "atoms", "has_atoms", "type_names",
    "has_type_names", "values", "has_values", "aux_values",
    "has_aux_values", "modes", "indents", "provenance_texts", "atom_table",
]
traversal_fields = ["kinds", "first_children", "child_counts", "parents", "children"]

def function_body(text, name):
    start = text.index(f"func {name}(")
    brace = text.index("{", start)
    depth = 0
    for pos in range(brace, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[start:pos + 1]
    raise SystemExit(f"unterminated function body: {name}")

early_body = function_body(
    ast_lifetime,
    "SelfMirAstArenaNonTraversalStorageRetireAfterDomainProjection",
)
final_body = function_body(
    ast_lifetime,
    "SelfMirAstArenaTraversalStorageRetireAfterRoutineFacts",
)
for field in non_traversal_fields:
    if early_body.count(f"artifact.arena.{field};") != 1:
        raise SystemExit(f"typed-AST early leaf coverage drift: {field}")
for field in traversal_fields:
    if early_body.count(f"artifact.arena.{field};") != 1:
        raise SystemExit(f"typed-AST traversal carry drift: {field}")
    if final_body.count(f"artifact.arena.{field};") != 1:
        raise SystemExit(f"typed-AST final leaf coverage drift: {field}")
early_drops = re.findall(r"CompilerRetireArrayStorage\((\w+)\);", early_body)
final_drops = re.findall(r"CompilerRetireArrayStorage\((\w+)\);", final_body)
if len(early_drops) != 13 or len(set(early_drops)) != 13:
    raise SystemExit("typed-AST non-traversal backings are not retired exactly once")
if len(final_drops) != 5 or len(set(final_drops)) != 5:
    raise SystemExit("typed-AST traversal backings are not retired exactly once")
if set(early_drops) & set(final_drops):
    raise SystemExit("typed-AST early/final retirement sets overlap")
if "AstArena(" not in early_body or "let empty_ints: Array<Int> = [];" not in early_body \
        or "let empty_strings: Array<String> = [];" not in early_body:
    raise SystemExit("typed-AST early owner leaves dangling retired descriptors")
for body in (early_body, final_body):
    if "ArrayDropOwnedStrings" in body:
        raise SystemExit("typed-AST lifetime owner reaches shared elements or graph facts")
if early_body.count("artifact.expression_graphs;") != 1 or \
        "artifact.expression_graphs" in final_body:
    raise SystemExit("typed-AST expression graph carry drift")
if re.search(r"CompilerRetireArrayStorage\([^)]*expression_graph", ast_lifetime):
    raise SystemExit("typed-AST lifetime owner retires expression graph storage")
all_ast_drops = list(re.finditer(r"CompilerRetireArrayStorage\(", ast_lifetime))
if len(all_ast_drops) != 18:
    raise SystemExit("typed-AST lifetime owner gained an unowned retirement call")
early_start = ast_lifetime.index(early_body)
final_start = ast_lifetime.index(final_body)
allowed_spans = (
    (early_start, early_start + len(early_body)),
    (final_start, final_start + len(final_body)),
)
for call in all_ast_drops:
    if not any(start <= call.start() < end for start, end in allowed_spans):
        raise SystemExit("typed-AST retirement call escaped the two admitted owners")

normalized_early = re.sub(r"\s+", "", early_body)
expected_arena = (
    "AstArena(kinds,empty_ints,empty_ints,empty_ints,"
    "empty_ints,empty_ints,empty_ints,empty_ints,empty_ints,empty_ints,"
    "empty_ints,first_children,child_counts,parents,empty_ints,"
    "empty_strings,children,empty_strings)"
)
if normalized_early.count(expected_arena) != 1:
    raise SystemExit("typed-AST reduced arena positional mapping drift")
expected_artifact = (
    "AstTreeArtifact(tree_text," + expected_arena +
    ",count,expression_graphs,identity_digest)"
)
if normalized_early.count(expected_artifact) != 1:
    raise SystemExit("typed-AST reduced artifact positional mapping drift")

artifact_function = function_body(
    artifact, "SelfMirProgramFactsBeforeCanonicalIdsObserved"
)
domain_done = artifact_function.index(
    'SelfMirArtifactPressureStage(observe_pressure, "domain-projection:done");'
)
early_retire = artifact_function.index(
    "SelfMirAstArenaNonTraversalStorageRetireAfterDomainProjection(artifact);"
)
routine_input = artifact_function.index("let input: SelfMirRoutineInput")
intent_done = artifact_function.index(
    'SelfMirArtifactPressureStage(observe_pressure, "intent-lowering:done");'
)
final_retire = artifact_function.index(
    "SelfMirAstArenaTraversalStorageRetireAfterRoutineFacts("
)
success_final_retire = artifact_function.rindex(
    "SelfMirAstArenaTraversalStorageRetireAfterRoutineFacts("
)
return_facts = artifact_function.rindex("return facts;")
if not (domain_done < early_retire < routine_input < intent_done < success_final_retire < return_facts):
    raise SystemExit("typed-AST retirement is not domain < 13-drop < routines < 5-drop")
after_early_call = early_retire + len(
    "SelfMirAstArenaNonTraversalStorageRetireAfterDomainProjection(artifact);"
)
post_early = artifact_function[after_early_call:success_final_retire]
if re.search(r"\bartifact\.", post_early):
    raise SystemExit("consumed full artifact is read after early retirement")
for forbidden in (
    "NominalSubkind(", "TypeName(", "ValueText(",
    "AuxValueText(", "Mode(", "Indent(", "ProvenanceText(",
    "AstTreeArtifactReady(", "TypedAstArenaParallelRowsReady(",
):
    if forbidden in post_early:
        raise SystemExit(f"retired typed-AST lane regained a consumer: {forbidden}")
returns = list(re.finditer(
    r"(?m)^\s*return\b", artifact_function[after_early_call:]
))
cleanup_calls = list(re.finditer(
    r"SelfMirAstArenaTraversalStorageRetireAfterRoutineFacts\(",
    artifact_function[after_early_call:],
))
if len(returns) != 6 or len(cleanup_calls) != 6:
    raise SystemExit("typed-AST success/failure cleanup cardinality drift")
prior_return = -1
for returned in returns:
    if not any(prior_return < call.start() < returned.start() for call in cleanup_calls):
        raise SystemExit("typed-AST failure return bypasses traversal cleanup")
    prior_return = returned.start()
success_final_end = artifact_function.index(");", success_final_retire) + 2
post_final = artifact_function[success_final_end:return_facts]
for forbidden in ("input.", "traversal_artifact.", "TypedAstArena", "AstTreeArtifact"):
    if forbidden in post_final:
        raise SystemExit(f"retired traversal carrier regained a consumer: {forbidden}")

routine_match = (
    root / "src/self_hosted/mir/routine_match_owner.pgy"
).read_text(encoding="utf-8")
match_fact_body = function_body(routine_match, "SelfMirMatchCaseFactForNode")
for required in (
    "SemanticAstStatementIndexForNode(",
    "input.analysis.statements.payload_texts[index]",
    "SelfMirMatchCaseFactFromText(",
):
    if required not in match_fact_body:
        raise SystemExit(f"match pattern statement-fact projection misses {required}")
for forbidden in ("input.artifact", "SelfMirMatchCaseFactFromArtifact("):
    if forbidden in match_fact_body:
        raise SystemExit(f"match pattern regained typed-AST atom read: {forbidden}")

for path in (root / "src/self_hosted/mir").glob("routine*.pgy"):
    text = path.read_text(encoding="utf-8")
    for forbidden in (
        "TypedAstArenaNominalSubkind(", "TypedAstArenaTypeName(",
        "TypedAstArenaValueText(", "TypedAstArenaAuxValueText(",
        "TypedAstArenaMode(", "TypedAstArenaIndent(",
        "TypedAstArenaProvenanceText(", "AstTreeArtifactReady(",
    ):
        if forbidden in text:
            raise SystemExit(f"routine closure reads retired AST lane: {path} {forbidden}")

source_mir = source_mir_path.read_text(encoding="utf-8")
projection_call = source_mir.index("SelfMirProgramFactsBeforeCanonicalIdsObserved(")
canonical_ids = source_mir.index(
    "mir_facts = SelfMirCanonicalInstructionIds(mir_facts);", projection_call
)
facts_ready = source_mir.index("if !SelfMirProgramFactsReady(mir_facts)", canonical_ids)
if not (projection_call < canonical_ids < facts_ready):
    raise SystemExit("source-MIR canonicalization ordering drift")
if "DriverRung2AstArenaStorageRetireAfterMirFactRows" in source_mir:
    raise SystemExit("driver regained late whole-arena retirement")
if "let artifact: AstTreeArtifact = CompileSourceToAstArtifact(source_path);" not in source_mir:
    raise SystemExit("source-to-MIR ownership boundary lost its named artifact binding")
if re.search(
    r"CompileArtifactToMirJsonVerified\(\s*CompileSourceToAstArtifact\(",
    source_mir
):
    raise SystemExit("source-to-MIR ownership boundary regained an unnamed artifact")

routine_build = routine_build_path.read_text(encoding="utf-8")
if "ref versions: Array<Int>" not in routine_build:
    raise SystemExit("local-version restore must borrow one named snapshot")
if "SelfMirLocalVersionsSnapshot(versions)" in routine_build:
    raise SystemExit("local-version restore reintroduced a replacement backing")
if "ArraySet(restored, i, version);" not in routine_build:
    raise SystemExit("local-version restore no longer updates the active backing")
if "ArrayPop(names); ArrayPop(types); ArrayPop(versions); ArrayPop(refs);" not in routine_build:
    raise SystemExit("local scope retirement no longer truncates active backings")
for path in (
    root / "src/self_hosted/mir/routine_if_owner.pgy",
    root / "src/self_hosted/mir/routine_match_owner.pgy",
):
    text = path.read_text(encoding="utf-8")
    if re.search(
        r"SelfMirRoutineAtLocalVersions\(\s*build,\s*"
        r"SelfMirLocalVersionsSnapshot\(", text
    ):
        raise SystemExit(f"local-version caller rebuilt a named snapshot: {path}")

call_sites = []
for path in (root / "src/self_hosted").rglob("*.pgy"):
    if "CompilerRetireArrayStorage(" in path.read_text(encoding="utf-8"):
        call_sites.append(path.relative_to(root).as_posix())
if call_sites != [
    "src/self_hosted/mir/ast_arena_storage_lifetime_owner.pgy",
    "src/self_hosted/mir/routine_build_storage_lifetime_owner.pgy",
]:
    raise SystemExit(f"compiler storage retirement call-site drift: {call_sites}")

approved_internal_owners = {
    "SelfMirRoutineBuildStorageRetireAfterLastConsumer",
    "SelfMirAstArenaNonTraversalStorageRetireAfterDomainProjection",
    "SelfMirAstArenaTraversalStorageRetireAfterRoutineFacts",
}
native_admission = (
    root / "src/semantic/type_checker_builtins_stdlib_array.c"
).read_text(encoding="utf-8")
native_names = set(re.findall(
    r'strcmp\(function_name,\s*"([A-Za-z0-9_]+)"\)', native_admission
))
if native_names != approved_internal_owners:
    raise SystemExit(f"native compiler-internal owner set drift: {native_names}")
for path in (
    "src/self_hosted/mir/routine_build_storage_lifetime_owner.pgy",
    "src/self_hosted/mir/ast_arena_storage_lifetime_owner.pgy",
):
    if native_admission.count(f'"{path}"') != 1:
        raise SystemExit(f"native compiler-internal module path drift: {path}")
self_host_admission = (
    root / "src/self_hosted/semantic/ast_expression_graph_collection_mutation_owner.pgy"
).read_text(encoding="utf-8")
caller_body = function_body(
    self_host_admission, "SemanticCompilerRetireArrayStorageCallerReady"
)
self_host_names = set(re.findall(
    r'UnwrapOption\(name\)\s*==\s*"([A-Za-z0-9_]+)"', caller_body
))
if self_host_names != approved_internal_owners:
    raise SystemExit(f"self-host compiler-internal owner set drift: {self_host_names}")

runtime_owner = (
    root / "src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy"
).read_text(encoding="utf-8")
drop_emit_lines = [
    line for line in runtime_owner.splitlines()
    if "block =" in line and "CollectionRuntimeCDropStorageFn(" in line
]
if len(drop_emit_lines) != 2:
    raise SystemExit("self-host storage-drop emitter count drift")
for line in drop_emit_lines:
    for required in ("free(a->data)", "a->data = NULL", "a->len = 0", "a->cap = 0"):
        if required not in line:
            raise SystemExit(f"self-host storage-drop emitter misses {required}")
    for forbidden in ("for (", "a->data[i]", "free((void *)"):
        if forbidden in line:
            raise SystemExit(f"self-host storage-drop emitter frees an element: {forbidden}")

native_runtime = (
    root / "src/runtime/pgy_runtime_memory_array_slot_inline.h"
).read_text(encoding="utf-8")
native_start = native_runtime.index("pgy_array_drop_##SuffixName")
native_end = native_runtime.index("pgy_array_reserve_##SuffixName", native_start)
native_drop = native_runtime[native_start:native_end]
for required in ("pgy_free(arr->allocator, arr->data", "arr->data = NULL", "arr->length = 0", "arr->capacity = 0"):
    if required not in native_drop:
        raise SystemExit(f"native storage-drop runtime misses {required}")
for forbidden in ("for (", "arr->data["):
    if forbidden in native_drop:
        raise SystemExit(f"native storage-drop runtime frees an element: {forbidden}")

raw_runtime = (
    root / "src/runtime/pgy_runtime_lib_raw_array_exports.h"
).read_text(encoding="utf-8")
raw_start = raw_runtime.index("pgy_array_drop_storage_raw_export")
raw_end = raw_runtime.index("arr->capacity = 0;", raw_start) + len("arr->capacity = 0;")
raw_drop = raw_runtime[raw_start:raw_end]
for required in ("pgy_free(arr->allocator, arr->data", "arr->data = NULL", "arr->length = 0", "arr->capacity = 0"):
    if required not in raw_drop:
        raise SystemExit(f"LLVM raw storage-drop runtime misses {required}")
for forbidden in ("for (", "arr->data["):
    if forbidden in raw_drop:
        raise SystemExit(f"LLVM raw storage-drop runtime frees an element: {forbidden}")

sorted_tables = (
    (root / "src/common/pgy_builtin_type_table.c", r'\{ "([A-Za-z0-9_]+)", "'),
    (root / "src/semantic/type_checker_builtins_stdlib_collections.c", r'\{ "([A-Za-z0-9_]+)", STDLIB_COLLECTION_'),
    (root / "src/codegen/llvm_expr_array_calls.c", r'\{"([A-Za-z0-9_]+)", \d+, LLVM_ARRAY_BUILTIN_'),
    (root / "src/codegen/transpiler_expr_stdlib_builtin_policy.c", r'\{"([A-Za-z0-9_]+)", \d+, TRANSPILER_ARRAY_OP_'),
)
for path, pattern in sorted_tables:
    names = re.findall(pattern, path.read_text(encoding="utf-8"))
    if "CompilerRetireArrayStorage" not in names or names != sorted(names):
        raise SystemExit(f"compiler retirement registry ordering drift: {path}")

print("[routine-build-storage-lifetime] structural coverage ok: 41 Int + 43 String")
PY

grep -Fq 'PGY_BUILTIN_FLAG_COMPILER_INTERNAL' \
    "$ROOT_DIR/src/common/pgy_builtin_type_table.h" || {
    echo "[routine-build-storage-lifetime] compiler-internal builtin flag is missing" >&2
    exit 1
}
grep -Fq '{ "CompilerRetireArrayStorage", "Void",' \
    "$ROOT_DIR/src/common/pgy_builtin_type_table.c" || {
    echo "[routine-build-storage-lifetime] compiler storage retirement registry row is missing" >&2
    exit 1
}
grep -Fq 'pgy_array_drop_storage_raw_export' \
    "$ROOT_DIR/src/codegen/llvm_runtime_raw_collections.c" || {
    echo "[routine-build-storage-lifetime] LLVM raw drop runtime is not registered" >&2
    exit 1
}
grep -Fq 'sym->is_consumed = true;' \
    "$ROOT_DIR/src/semantic/type_checker_expr_names.c" || {
    echo "[routine-build-storage-lifetime] native Array storage consumption is missing" >&2
    exit 1
}
grep -Fq 'callee == "CompilerRetireArrayStorage"' \
    "$ROOT_DIR/src/self_hosted/semantic/collection_mutation_policy_owner.pgy" || {
    echo "[routine-build-storage-lifetime] self-host mutation policy is missing" >&2
    exit 1
}
if grep -R -Fq 'CompilerRetireArrayStorage' "$ROOT_DIR/src/lsp"; then
    echo "[routine-build-storage-lifetime] compiler-internal retirement leaked into LSP surface" >&2
    exit 1
fi

if "$PGY" "$NEGATIVE" --native-pipeline -o "$BUILD_DIR/use_after.exe" \
    >"$BUILD_DIR/negative.compile.out" 2>"$BUILD_DIR/negative.compile.err"; then
    echo "[routine-build-storage-lifetime] external retirement call did not fail closed" >&2
    exit 1
fi
grep -Fq 'restricted to self-host storage lifetime owners' \
    "$BUILD_DIR/negative.compile.err" || {
    cat "$BUILD_DIR/negative.compile.err" >&2
    echo "[routine-build-storage-lifetime] missing compiler-internal diagnostic" >&2
    exit 1
}

if "$PGY" "$IMPERSONATION_NEGATIVE" --native-pipeline \
    -o "$BUILD_DIR/impersonation.exe" \
    >"$BUILD_DIR/impersonation.compile.out" \
    2>"$BUILD_DIR/impersonation.compile.err"; then
    echo "[routine-build-storage-lifetime] owner impersonation did not fail closed" >&2
    exit 1
fi
grep -Fq 'restricted to self-host storage lifetime owners' \
    "$BUILD_DIR/impersonation.compile.err" || {
    cat "$BUILD_DIR/impersonation.compile.err" >&2
    echo "[routine-build-storage-lifetime] missing wrong-path owner diagnostic" >&2
    exit 1
}

"$PGY" "$OWNER" --native-pipeline --ast \
    >"$BUILD_DIR/owner.ast" 2>"$BUILD_DIR/owner.ast.err"
grep -Fq 'CompilerRetireArrayStorage(local_refs)' "$BUILD_DIR/owner.ast" || {
    echo "[routine-build-storage-lifetime] semantic owner AST is incomplete" >&2
    exit 1
}
"$PGY" "$MATCH_OWNER" --native-pipeline --ast \
    >"$BUILD_DIR/routine_match.ast" 2>"$BUILD_DIR/routine_match.ast.err"
[[ -s "$BUILD_DIR/routine_match.ast" ]] || {
    echo "[routine-build-storage-lifetime] match restore closure is incomplete" >&2
    exit 1
}
"$PGY" "$AST_LIFETIME_OWNER" --native-pipeline --ast \
    >"$BUILD_DIR/source_mir.ast" 2>"$BUILD_DIR/source_mir.ast.err"
grep -Fq 'SelfMirAstArenaNonTraversalStorageRetireAfterDomainProjection' \
    "$BUILD_DIR/source_mir.ast" || {
    echo "[routine-build-storage-lifetime] source-MIR artifact lifetime AST is incomplete" >&2
    exit 1
}
"$PGY" "$OWNER" --native-pipeline --emit-c -o "$BUILD_DIR/owner.c" \
    >"$BUILD_DIR/owner.c.out" 2>"$BUILD_DIR/owner.c.err"
grep -Fq 'pgy_array_drop_Int(&_pgy_ssa_block_ids_' "$BUILD_DIR/owner.c" || {
    echo "[routine-build-storage-lifetime] C Int backing retirement is missing" >&2
    exit 1
}
grep -Fq 'pgy_array_drop_String(&_pgy_ssa_instruction_kinds_' "$BUILD_DIR/owner.c" || {
    echo "[routine-build-storage-lifetime] C String backing retirement is missing" >&2
    exit 1
}

if [[ -n "${PGY_ROUTINE_BUILD_STORAGE_LLVM_BIN:-}" ]]; then
    LLVM_PGY="$PGY_ROUTINE_BUILD_STORAGE_LLVM_BIN"
    if [[ "$LLVM_PGY" != *.exe && -x "${LLVM_PGY}.exe" ]]; then
        LLVM_PGY="${LLVM_PGY}.exe"
    fi
    "$LLVM_PGY" "$OWNER" --native-pipeline --emit-llvm \
        -o "$BUILD_DIR/owner.ll" \
        >"$BUILD_DIR/llvm.compile.out" 2>"$BUILD_DIR/llvm.compile.err"
    grep -Fq 'pgy_array_drop_storage_raw_export' "$BUILD_DIR/owner.ll" || {
        echo "[routine-build-storage-lifetime] LLVM backing retirement is missing" >&2
        exit 1
    }
fi

echo "[routine-build-storage-lifetime] PASS"

#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PYTHON="${PYTHON:-python}"
BUILD_DIR="${PGY_ROUTINE_BUILD_STORAGE_TEST_DIR:-$ROOT_DIR/.tmp/self_hosted/routine_build_storage_lifetime}"
OWNER="$ROOT_DIR/src/self_hosted/mir/routine_build_storage_lifetime_owner.pgy"
MATCH_OWNER="$ROOT_DIR/src/self_hosted/mir/routine_match_owner.pgy"
AST_LIFETIME_OWNER="$ROOT_DIR/src/self_hosted/mir/ast_arena_storage_lifetime_owner.pgy"
BODY_LIFETIME_OWNER="$ROOT_DIR/src/self_hosted/mir/body_type_bundle_storage_lifetime_owner.pgy"
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

"$PYTHON" \
    "$ROOT_DIR/scripts/render_compiler_internal_builtin_caller_registry.py" \
    "$ROOT_DIR/src/common/compiler_internal_builtin_caller_registry.def" \
    "$ROOT_DIR/src/self_hosted/semantic/compiler_internal_builtin_caller_registry_owner.pgy" \
    --check

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
"$PGY" "$BODY_LIFETIME_OWNER" --native-pipeline --ast \
    >"$BUILD_DIR/body_type_lifetime.ast" \
    2>"$BUILD_DIR/body_type_lifetime.ast.err"
grep -Fq 'CompilerRetireArrayStorage(generic_actual_types)' \
    "$BUILD_DIR/body_type_lifetime.ast" || {
    echo "[routine-build-storage-lifetime] body-type lifetime AST is incomplete" >&2
    exit 1
}
"$PGY" "$BODY_LIFETIME_OWNER" --native-pipeline --emit-c \
    -o "$BUILD_DIR/body_type_lifetime.c" \
    >"$BUILD_DIR/body_type_lifetime.c.out" \
    2>"$BUILD_DIR/body_type_lifetime.c.err"
grep -Fq 'pgy_array_drop_Int(&_pgy_ssa_initializer_node_ids_' \
    "$BUILD_DIR/body_type_lifetime.c" || {
    echo "[routine-build-storage-lifetime] body-type Int backing retirement is missing" >&2
    exit 1
}
grep -Fq 'pgy_array_drop_String(&_pgy_ssa_generic_actual_types_' \
    "$BUILD_DIR/body_type_lifetime.c" || {
    echo "[routine-build-storage-lifetime] body-type String backing retirement is missing" >&2
    exit 1
}
for local in capability_deferred_uses capability_unknown_call_effects; do
    grep -Fq "pgy_array_drop_Bool(&_pgy_ssa_${local}_" \
        "$BUILD_DIR/body_type_lifetime.c" || {
        echo "[routine-build-storage-lifetime] body-type Bool backing retirement is missing: $local" >&2
        exit 1
    }
done
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

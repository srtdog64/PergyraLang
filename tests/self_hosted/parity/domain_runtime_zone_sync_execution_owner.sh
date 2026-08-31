#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/domain-runtime-zone-sync}"
PGY_BIN="${PGY_BIN:-$ROOT_DIR/bin/pgy.exe}"
SELF_DRIVER="${PGY_SELFHOST_DRIVER_BIN:-${PGY_SELF_DRIVER_BIN:-}}"
if [[ -z "$SELF_DRIVER" ]]; then
    if [[ -x "$ROOT_DIR/bin/pgy-self-driver.exe" ]]; then
        SELF_DRIVER="$ROOT_DIR/bin/pgy-self-driver.exe"
    else
        SELF_DRIVER="$ROOT_DIR/bin/pgy-self-driver"
    fi
fi
CODEGEN_BIN="${PGY_SELFHOST_PREBUILT_CODEGEN:-$BUILD_DIR/codegen.exe}"
CC_BIN="${CC:-gcc}"
POSIX_FEATURE_FLAGS=(
    -D_POSIX_C_SOURCE=200809L
    -D_XOPEN_SOURCE=700
    -D_DEFAULT_SOURCE
)

mkdir -p "$BUILD_DIR"
cd "$ROOT_DIR"

# zero-topology typed receipt and exact zone-definition bijection
# zone identity bijection and single/thread-safe explicit-lifecycle execution
compare_zone_bijection() {
    local ast_file="$1"
    local c_file="$2"
    local label="$3"
    local declaration_names="$BUILD_DIR/$label-zone-declarations.txt"
    local definition_names="$BUILD_DIR/$label-zone-definitions.txt"

    sed -nE \
        's/^  (\[export\] )?Zone: ([A-Za-z_][A-Za-z0-9_]*).*$/\2/p' \
        "$ast_file" >"$declaration_names"
    sed -nE \
        's/^static void ([A-Za-z_][A-Za-z0-9_]*)_sync\(.*/\1/p' \
        "$c_file" >"$definition_names"
    if [[ -s "$declaration_names" ]] && \
        [[ -n "$(sort "$declaration_names" | uniq -d)" ]]; then
        echo "$label AST duplicates a zone declaration identity" >&2
        exit 1
    fi
    if [[ -s "$definition_names" ]] && \
        [[ -n "$(sort "$definition_names" | uniq -d)" ]]; then
        echo "$label C duplicates a zone sync definition identity" >&2
        exit 1
    fi
    diff -u \
        <(sort "$declaration_names") \
        <(sort "$definition_names")
}

if [[ -z "${PGY_SELFHOST_PREBUILT_CODEGEN:-}" ]]; then
    CODEGEN_C="$BUILD_DIR/codegen.c"
    "$SELF_DRIVER" --emit-c-artifact-verified \
        src/self_hosted/codegen/main.pgy "${CODEGEN_C#"$ROOT_DIR/"}"
    "$CC_BIN" -x c -std=c11 -O3 -fwrapv -fno-strict-aliasing \
        -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
        "$CODEGEN_C" -o "$CODEGEN_BIN"
fi
[[ -x "$CODEGEN_BIN" ]]

ZERO_SOURCE="$ROOT_DIR/tests/self_hosted/fixtures/domain_runtime_zone_sync_zero.pgy"
ZERO_AST="$BUILD_DIR/zero.ast.txt"
ZERO_C="$BUILD_DIR/zero.c"
ZERO_BIN="$BUILD_DIR/zero.exe"
ZERO_THREADSAFE_BIN="$BUILD_DIR/zero-threadsafe.exe"
ZERO_HARNESS="$ROOT_DIR/tests/self_hosted/parity/fixture/domain_runtime_zone_sync_harness.c"

"$PGY_BIN" --native-pipeline --ast "$ZERO_SOURCE" >"$ZERO_AST"
"$CODEGEN_BIN" --source "${ZERO_SOURCE#"$ROOT_DIR/"}" >"$ZERO_C"
compare_zone_bijection "$ZERO_AST" "$ZERO_C" zero

grep -Fq '#include "pgy_runtime_zone_sync_abi.h"' "$ZERO_C"
grep -Fq '#include "pgy_frontier_policy.h"' "$ZERO_C"
grep -Fq 'PGY_ZONE_LOCK_FIELD' "$ZERO_C"
grep -Fq 'PGY_ZONE_GENERATION_FIELD' "$ZERO_C"
grep -Fq 'static void EmptyZone_sync(EmptyZone *self)' "$ZERO_C"
grep -Fq 'PGY_ZONE_WRLOCK(self);' "$ZERO_C"
grep -Fq 'PGY_ZONE_GENERATION_INC(self);' "$ZERO_C"
grep -Fq '_pgy_zone_frontier_pass_limit = 1;' "$ZERO_C"
grep -Fq 'PGY_FRONTIER_REASON_ZONE_OVERFLOW' "$ZERO_C"
grep -Fq 'PGY_ZONE_UNLOCK(self);' "$ZERO_C"
for field in \
    __projection_ready_view __projection_dirty_view \
    __projection_epoch_view __projection_cause_view \
    __projection_ready_packet __projection_dirty_packet \
    __projection_epoch_packet __projection_cause_packet; do
    grep -Fq "$field" "$ZERO_C"
done

"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    "${POSIX_FEATURE_FLAGS[@]}" \
    -I "$ROOT_DIR/src" -I "$ROOT_DIR/src/runtime" -pthread \
    -DPGY_ZONE_SYNC_GENERATED_C=\"$ZERO_C\" \
    "$ZERO_HARNESS" -o "$ZERO_BIN"
"$ZERO_BIN"
"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    "${POSIX_FEATURE_FLAGS[@]}" \
    -I "$ROOT_DIR/src" -I "$ROOT_DIR/src/runtime" -pthread \
    -DPGY_ZONE_THREADSAFE \
    -DPGY_ZONE_SYNC_GENERATED_C=\"$ZERO_C\" \
    "$ZERO_HARNESS" -o "$ZERO_THREADSAFE_BIN"
"$ZERO_THREADSAFE_BIN"

# A generated fresh local owns its hidden lock for the exact lexical scope.
# This is implementation-resource cleanup, not a source-level world lifetime.
LIFECYCLE_SOURCE="$ROOT_DIR/tests/self_hosted/fixtures/domain_runtime_zone_lifecycle.pgy"
LIFECYCLE_AST="$BUILD_DIR/lifecycle.ast.txt"
LIFECYCLE_C="$BUILD_DIR/lifecycle.c"
LIFECYCLE_BIN="$BUILD_DIR/lifecycle.exe"
LIFECYCLE_THREADSAFE_BIN="$BUILD_DIR/lifecycle-threadsafe.exe"
LIFECYCLE_SINGLE_OUT="$BUILD_DIR/lifecycle-single.out"
LIFECYCLE_THREADSAFE_OUT="$BUILD_DIR/lifecycle-threadsafe.out"
LIFECYCLE_EXPECTED="$BUILD_DIR/lifecycle.expected"

"$PGY_BIN" --native-pipeline --ast "$LIFECYCLE_SOURCE" >"$LIFECYCLE_AST"
"$CODEGEN_BIN" --source "${LIFECYCLE_SOURCE#"$ROOT_DIR/"}" >"$LIFECYCLE_C"
compare_zone_bijection "$LIFECYCLE_AST" "$LIFECYCLE_C" lifecycle
[[ "$(grep -Fc 'PGY_ZONE_LOCK_INIT(&' "$LIFECYCLE_C")" == 7 ]]
[[ "$(grep -Fc 'PGY_ZONE_LOCK_DESTROY(&' "$LIFECYCLE_C")" -ge 7 ]]
grep -Fq 'CounterZone_sync(self);' "$LIFECYCLE_C"
grep -Fq 'PGY_ZONE_LOCK_INIT(&counter);' "$LIFECYCLE_C"
grep -Fq 'PGY_ZONE_LOCK_DESTROY(&counter);' "$LIFECYCLE_C"
grep -Fq 'PGY_ZONE_LOCK_DESTROY(&early);' "$LIFECYCLE_C"
grep -Fq 'PGY_ZONE_LOCK_DESTROY(&nested);' "$LIFECYCLE_C"
grep -Fq 'PGY_ZONE_LOCK_DESTROY(&breaker);' "$LIFECYCLE_C"
grep -Fq 'PGY_ZONE_LOCK_DESTROY(&continuing);' "$LIFECYCLE_C"
grep -Fq 'PGY_ZONE_LOCK_INIT(&cloned);' "$LIFECYCLE_C"
grep -Fq 'PGY_ZONE_LOCK_DESTROY(&cloned);' "$LIFECYCLE_C"
grep -Fq 'PGY_ZONE_LOCK_DESTROY(&original);' "$LIFECYCLE_C"
init_line="$(grep -nF 'PGY_ZONE_LOCK_INIT(&counter);' "$LIFECYCLE_C" | cut -d: -f1)"
use_line="$(grep -nF 'CounterZone_Value(&(counter))' "$LIFECYCLE_C" | cut -d: -f1)"
destroy_line="$(grep -nF 'PGY_ZONE_LOCK_DESTROY(&counter);' "$LIFECYCLE_C" | cut -d: -f1)"
[[ "$init_line" -lt "$use_line" && "$use_line" -lt "$destroy_line" ]]
breaker_destroy_line="$(grep -nF 'PGY_ZONE_LOCK_DESTROY(&breaker);' "$LIFECYCLE_C" | head -1 | cut -d: -f1)"
break_line="$(grep -nF 'break;' "$LIFECYCLE_C" | cut -d: -f1)"
continuing_destroy_line="$(grep -nF 'PGY_ZONE_LOCK_DESTROY(&continuing);' "$LIFECYCLE_C" | head -1 | cut -d: -f1)"
continue_line="$(grep -nF 'continue;' "$LIFECYCLE_C" | cut -d: -f1)"
[[ "$breaker_destroy_line" -lt "$break_line" ]]
[[ "$continuing_destroy_line" -lt "$continue_line" ]]

printf '7\n11\n13\n17\n19\n23\n' >"$LIFECYCLE_EXPECTED"
"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    "${POSIX_FEATURE_FLAGS[@]}" \
    -I "$ROOT_DIR/src" -I "$ROOT_DIR/src/runtime" -pthread \
    "$LIFECYCLE_C" -o "$LIFECYCLE_BIN"
"$LIFECYCLE_BIN" | tr -d '\r' >"$LIFECYCLE_SINGLE_OUT"
cmp -s "$LIFECYCLE_EXPECTED" "$LIFECYCLE_SINGLE_OUT"
"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    "${POSIX_FEATURE_FLAGS[@]}" \
    -I "$ROOT_DIR/src" -I "$ROOT_DIR/src/runtime" -pthread \
    -DPGY_ZONE_THREADSAFE "$LIFECYCLE_C" -o "$LIFECYCLE_THREADSAFE_BIN"
"$LIFECYCLE_THREADSAFE_BIN" | tr -d '\r' >"$LIFECYCLE_THREADSAFE_OUT"
cmp -s "$LIFECYCLE_EXPECTED" "$LIFECYCLE_THREADSAFE_OUT"

source "$ROOT_DIR/tests/self_hosted/parity/domain_runtime_zone_value_carriage_admission_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/domain_runtime_zone_parameter_admission_owner.sh"

NONZERO_SOURCE="$ROOT_DIR/tests/cases/backend_compare/zone_layer_projection_runtime/main.pgy"
NONZERO_AST="$BUILD_DIR/nonzero.ast.txt"
NONZERO_OUT="$BUILD_DIR/nonzero.out"
NONZERO_ERR="$BUILD_DIR/nonzero.err"

"$PGY_BIN" --native-pipeline --ast "$NONZERO_SOURCE" >"$NONZERO_AST"
if "$CODEGEN_BIN" --source "${NONZERO_SOURCE#"$ROOT_DIR/"}" \
    >"$NONZERO_OUT" 2>"$NONZERO_ERR"; then
    echo "nonzero zone topology must fail closed in self-host C" >&2
    exit 1
fi
grep -Fq 'nonzero domain runtime zone topology is not executable in self-host C' \
    "$NONZERO_OUT" "$NONZERO_ERR"
if grep -Eq '^typedef struct|^static void .*Zone_sync\(' "$NONZERO_OUT"; then
    echo "nonzero topology rejection leaked partial C" >&2
    exit 1
fi

if [[ -n "${PGY_SELFHOST_EXACT_AST:-}" || \
      -n "${PGY_SELFHOST_EXACT_C:-}" ]]; then
    [[ -n "${PGY_SELFHOST_EXACT_AST:-}" && \
       -n "${PGY_SELFHOST_EXACT_C:-}" ]]
    compare_zone_bijection \
        "$PGY_SELFHOST_EXACT_AST" "$PGY_SELFHOST_EXACT_C" exact
fi

BRIDGE_OWNER="$ROOT_DIR/src/self_hosted/compiler/domain_runtime_c_codegen_bridge_owner.pgy"
DRIVER_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_pipeline_owner.pgy"
[[ "$(grep -Fc 'func CompilerDomainRuntimeCZoneSyncBlock(' "$BRIDGE_OWNER")" == 1 ]]
if grep -Fq '"zone frontier recompute exceeded bounded pass limit"' \
    "$BRIDGE_OWNER"; then
    echo "self-host zone bridge duplicated the frontier-policy reason" >&2
    exit 1
fi
grep -Fq 'admitted.declarations' "$BRIDGE_OWNER"
if grep -Fq 'BuildMirProgramDeclarationIndex' "$BRIDGE_OWNER"; then
    echo "domain runtime C bridge reintroduced a declaration-root rescan" >&2
    exit 1
fi
grep -Fq 'GenerateCUnitFromAdmittedSemanticArtifact(' "$DRIVER_OWNER"
if grep -Fq 'CodegenDomainRuntimeFactsEmpty(true)' "$DRIVER_OWNER"; then
    echo "production driver reintroduced the empty domain-runtime fallback" >&2
    exit 1
fi

echo "[domain-runtime-zone-sync] PASS"

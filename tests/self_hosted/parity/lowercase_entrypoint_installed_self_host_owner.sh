#!/usr/bin/env bash
# Lowercase `func main()` is selected once by semantic signature facts.  The
# installed source-to-MIR and source-to-C paths consume that identity; this gate
# does not claim the separate direct-MIR backend envelope.
# Forbidden fallbacks: SemanticAstArtifactIsMainFunction,
# CodegenAstArenaIsMainFunction, entrypoint_name_rescan,
# lowercase_backend_exception.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-cc}"
WORK_REL=".tmp/self_hosted/lowercase_entrypoint"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE="tests/cases/backend_compare/entry_lowercase_main/main.pgy"
EXPECTED="$ROOT_DIR/tests/cases/backend_compare/entry_lowercase_main/expected.stdout"
SIGNATURE_CONTRACT="$ROOT_DIR/src/self_hosted/semantic/ast_signature_contract_owner.pgy"
PIPELINE_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_pipeline_owner.pgy"

fail() {
    echo "[self-host-lowercase-entrypoint] $*" >&2
    exit 1
}

require_text() {
    grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"
}

if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ "$SELF_DRIVER" != *.exe ]] &&
    pgy_binary_expects_windows_paths "${SELF_DRIVER}.exe"; then
    SELF_DRIVER="${SELF_DRIVER}.exe"
fi
[[ -x "$PGY" ]] || fail "missing public pgy launcher: $PGY"
[[ -x "$SELF_DRIVER" ]] || fail "missing installed self-host driver: $SELF_DRIVER"
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"

PGY="$(cd "$(dirname "$PGY")" && pwd -P)/$(basename "$PGY")"
SELF_DRIVER="$(cd "$(dirname "$SELF_DRIVER")" && pwd -P)/$(basename "$SELF_DRIVER")"
suffix=""
installed_name="pgy-self-driver"
if [[ "$PGY" == *.exe ]]; then
    suffix=".exe"
    installed_name="pgy-self-driver.exe"
fi
[[ "$SELF_DRIVER" == "$(dirname "$PGY")/$installed_name" ]] ||
    fail "self-host driver is not installed beside the public launcher"

compile_emitted_c() {
    local source="$1"
    local output="$2"
    pgy_selfhost_select_emitted_c_compile_profile ||
        fail "emitted C compile profile is invalid"
    local -a command=("$CC" -x c -std=c11)
    command+=("${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[@]}")
    if pgy_selfhost_emitted_c_uses_runtime_headers "$source"; then
        command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
    fi
    command+=("$source" -o "$output")
    "${command[@]}"
}

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/{source.mir.json,lowercase.c,lowercase.norm.c,lowercase-program*,lowercase.out} \
    "$WORK_DIR"/{worker.pgy,worker.c,worker.out,worker.err,uppercase.c,uppercase-program*,uppercase.out}

(cd "$ROOT_DIR" && unset PGY_NATIVE_PIPELINE &&
    "$SELF_DRIVER" --emit-mir-json-verified "$SOURCE" \
        -o "$WORK_REL/source.mir.json") \
    >"$WORK_DIR/mir.out" 2>"$WORK_DIR/mir.err" || {
        cat "$WORK_DIR/mir.out" "$WORK_DIR/mir.err" >&2 || true
        fail "installed self-host MIR producer rejected lowercase main"
    }
[[ -s "$WORK_DIR/source.mir.json" ]] || fail "self-host MIR artifact is missing"
[[ "$(grep -Fo '"name":"main"' "$WORK_DIR/source.mir.json" | wc -l | tr -d ' ')" == "1" ]] ||
    fail "self-host MIR did not preserve one lowercase entrypoint identity"
! grep -Fq '"name":"Main"' "$WORK_DIR/source.mir.json" ||
    fail "self-host MIR rewrote lowercase entrypoint identity"

(cd "$ROOT_DIR" && unset PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN &&
    "$PGY" "$SOURCE" --emit-c -o "$WORK_REL/lowercase.c") \
    >"$WORK_DIR/lowercase.emit.out" 2>"$WORK_DIR/lowercase.emit.err" || {
        cat "$WORK_DIR/lowercase.emit.out" "$WORK_DIR/lowercase.emit.err" >&2 || true
        fail "public installed source-C path rejected lowercase main"
    }
tr -d '\r' <"$WORK_DIR/lowercase.c" >"$WORK_DIR/lowercase.norm.c"
[[ "$(grep -Fxc 'void __pgy_user_main_lowercase(void);' "$WORK_DIR/lowercase.norm.c")" == "1" ]] ||
    fail "lowercase user-body prototype is not unique"
[[ "$(grep -Fxc 'void __pgy_user_main_lowercase(void)' "$WORK_DIR/lowercase.norm.c")" == "1" ]] ||
    fail "lowercase user-body definition is not unique"
[[ "$(grep -Fxc 'int main(void)' "$WORK_DIR/lowercase.norm.c")" == "1" ]] ||
    fail "host main wrapper is not unique"
[[ "$(grep -Fxc '    __pgy_user_main_lowercase();' "$WORK_DIR/lowercase.norm.c")" == "1" ]] ||
    fail "host wrapper does not call the lowercase user body exactly once"
compile_emitted_c "$WORK_DIR/lowercase.norm.c" "$WORK_DIR/lowercase-program$suffix"
"$WORK_DIR/lowercase-program$suffix" | tr -d '\r' >"$WORK_DIR/lowercase.out"
cmp -s "$EXPECTED" "$WORK_DIR/lowercase.out" ||
    fail "installed source-C lowercase runtime output drifted"

sed 's/^func main(/func worker(/' "$ROOT_DIR/$SOURCE" >"$WORK_DIR/worker.pgy"
set +e
(cd "$ROOT_DIR" && unset PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN &&
    "$PGY" "$WORK_REL/worker.pgy" --emit-c -o "$WORK_REL/worker.c") \
    >"$WORK_DIR/worker.out" 2>"$WORK_DIR/worker.err"
worker_rc=$?
set -e
[[ "$worker_rc" -ne 0 && ! -e "$WORK_DIR/worker.c" ]] ||
    fail "worker-only mutation emitted a C artifact"
grep -Fq "Code: entrypoint_cardinality" "$WORK_DIR/worker.out" "$WORK_DIR/worker.err" ||
    fail "worker-only mutation lost the entrypoint diagnostic"

require_text "$SIGNATURE_CONTRACT" \
    "crossed_selection.entrypoint_selection.signature_index = 0;"
require_text "$SIGNATURE_CONTRACT" \
    "crossed_spelling.entrypoint_selection.lowercase_spelling = true;"
require_text "$PIPELINE_OWNER" "SemanticAstFunctionSignatureFactsContractReady()"
require_text "$PIPELINE_OWNER" "if !CompilerDriverPipelineReady()"

(cd "$ROOT_DIR" && unset PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN &&
    "$PGY" examples/hello.pgy --emit-c -o "$WORK_REL/uppercase.c") \
    >"$WORK_DIR/uppercase.emit.out" 2>"$WORK_DIR/uppercase.emit.err" || {
        cat "$WORK_DIR/uppercase.emit.out" "$WORK_DIR/uppercase.emit.err" >&2 || true
        fail "uppercase Main regression failed to emit C"
    }
compile_emitted_c "$WORK_DIR/uppercase.c" "$WORK_DIR/uppercase-program$suffix"
"$WORK_DIR/uppercase-program$suffix" | tr -d '\r' >"$WORK_DIR/uppercase.out"
printf 'Hello, Pergyra!\n' >"$WORK_DIR/uppercase.expected"
cmp -s "$WORK_DIR/uppercase.expected" "$WORK_DIR/uppercase.out" ||
    fail "uppercase Main runtime regression drifted"

echo "[self-host-lowercase-entrypoint] semantic identity installed C runtime and artifact-free negative ratchets"

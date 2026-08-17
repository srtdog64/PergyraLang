#!/usr/bin/env bash
# Canonical runtime-call ABI IDs cross the self MIR wire and are consumed by
# both direct backends; missing, mismatched, forged, or mixed IDs fail closed.
# native/self MIR carry IDs 25/1/13 through direct C/LLVM and reject missing/mismatched/forged/mixed identities

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-intent-observability-mir-identity"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
CC="${CC:-gcc}"
CLANG="${CLANG:-clang}"
SOURCE_REL="tests/self_hosted/fixtures/intent_observability_history_count.pgy"
WORK_REL=".tmp/self_hosted/intent_observability_mir_identity"
WORK_DIR="$ROOT_DIR/$WORK_REL"
MUTATE="$ROOT_DIR/tests/self_hosted/parity/intent_observability_mir_identity_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL:native" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v python >/dev/null 2>&1 || fail "python is unavailable"
command -v "$CC" >/dev/null 2>&1 || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null 2>&1 || fail "clang is unavailable"
[[ -f "$MUTATE" ]] || fail "mutation owner is missing"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"
MIR_REL="$WORK_REL/source.mir.json"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$MIR_REL") >"$WORK_DIR/producer.out" 2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "self-host MIR producer rejected the fixture"
    }
python "$MUTATE" "$WORK_DIR/source.mir.json" verify \
    >"$WORK_DIR/identity.receipt"
grep -Fq 'intent-observability-mir-identity=ready' \
    "$WORK_DIR/identity.receipt" || fail "producer omitted canonical ABI IDs"
(cd "$ROOT_DIR" && "$PGY" --mir-json "$SOURCE_REL" --native-pipeline) \
    >"$WORK_DIR/native.mir.json" 2>"$WORK_DIR/native.producer.err" || {
        cat "$WORK_DIR/native.producer.err" >&2
        fail "native MIR oracle rejected the fixture"
    }
python "$MUTATE" "$WORK_DIR/native.mir.json" verify \
    >"$WORK_DIR/native.identity.receipt"

printf '0\nfalse\n\n' >"$WORK_DIR/expected.run"
"$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$WORK_DIR/runtime.o" \
    >"$WORK_DIR/runtime.compile.out" 2>"$WORK_DIR/runtime.compile.err" || {
        cat "$WORK_DIR/runtime.compile.err" >&2
        fail "LLVM runtime object did not compile"
    }

for producer in self native; do
  producer_mir="$MIR_REL"
  [[ "$producer" == native ]] && producer_mir="$WORK_REL/native.mir.json"
  for backend in c llvm; do
    artifact_rel="$WORK_REL/positive.$producer.$backend"
    artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/positive-$producer-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$producer_mir" -o "$artifact_rel") \
        >"$WORK_DIR/$producer.$backend.project.out" \
        2>"$WORK_DIR/$producer.$backend.project.err" || {
            cat "$WORK_DIR/$producer.$backend.project.err" >&2
            fail "$backend rejected $producer carried IDs"
        }
    [[ -s "$artifact" ]] || fail "$backend emitted no artifact"
    grep -Fq 'pgy_intent_history_count_export' "$artifact" ||
        fail "$backend did not resolve ABI ID 25 through the registry"
    if [[ "$backend" == c ]]; then
        command=("$CC" -x c -std=c11 -O2)
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=("$artifact" -lm -o "$bin")
    else
        command=("$CLANG" -x ir "$artifact" -x none \
            "$WORK_DIR/runtime.o" -pthread -lm -o "$bin")
    fi
    "${command[@]}" >"$WORK_DIR/$producer.$backend.compile.out" \
        2>"$WORK_DIR/$producer.$backend.compile.err" || {
            cat "$WORK_DIR/$producer.$backend.compile.err" >&2
            fail "$producer $backend artifact did not compile"
        }
    "$bin" 2>"$WORK_DIR/$producer.$backend.run.err" | tr -d '\r' \
        >"$WORK_DIR/$producer.$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$producer.$backend.run" ||
        fail "$producer $backend carried-ID execution drifted"
  done
done

for mutation in missing mismatch forged-non-observability syntax-conflict; do
    python "$MUTATE" "$WORK_DIR/source.mir.json" "$mutation" \
        "$WORK_DIR/$mutation.mir.json"
    for backend in c llvm; do
        output="$WORK_DIR/$mutation.$backend"
        rm -f "$output"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$WORK_REL/$mutation.mir.json" -o "$WORK_REL/$mutation.$backend") \
            >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation runtime ABI identity"
        fi
        [[ ! -e "$output" ]] || fail "$backend published $mutation output"
        grep -Fq 'CODEGEN ERROR:' "$WORK_DIR/$mutation.$backend.out" \
            "$WORK_DIR/$mutation.$backend.err" ||
            fail "$backend emitted no owned diagnostic for $mutation"
    done
done

echo "[$LABEL] native/self MIR -> direct C/LLVM stable ABI identity PASS"

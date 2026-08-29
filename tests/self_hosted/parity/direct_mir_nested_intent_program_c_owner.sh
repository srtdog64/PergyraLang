#!/usr/bin/env bash
# Reuses the admitted nested-intent MIR and negatives from the LLVM owner.
# Source-C and direct-MIR C must be one byte-identical shared-plan projection.

C_LABEL="self-host-direct-mir-nested-intent-program-c"
C_SOURCE_REL="$WORK_REL/source.c"
C_DIRECT_REL="$WORK_REL/direct.c"
C_SOURCE="$ROOT_DIR/$C_SOURCE_REL"
C_DIRECT="$ROOT_DIR/$C_DIRECT_REL"
CC="${CC:-cc}"

c_fail() { echo "[$C_LABEL] $*" >&2; exit 1; }
command -v "$CC" >/dev/null 2>&1 || c_fail "missing C compiler"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"

rm -f "$C_SOURCE" "$C_DIRECT" "$WORK_DIR/nested-c-program$suffix"
(cd "$ROOT_DIR" && "$DRIVER" --emit-c-artifact-verified \
    "$SOURCE_REL" "$C_SOURCE_REL") >"$WORK_DIR/source-c.out" \
    2>"$WORK_DIR/source-c.err" || {
        cat "$WORK_DIR/source-c.out" "$WORK_DIR/source-c.err" >&2
        c_fail "installed source-C substitution failed"
    }
(cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend=c \
    "$MIR_REL" -o "$C_DIRECT_REL") >"$WORK_DIR/direct-c.out" \
    2>"$WORK_DIR/direct-c.err" || {
        cat "$WORK_DIR/direct-c.out" "$WORK_DIR/direct-c.err" >&2
        c_fail "direct-MIR C substitution failed"
    }
[[ -s "$C_SOURCE" && -s "$C_DIRECT" ]] ||
    c_fail "C substitution omitted an artifact"
[[ ! -s "$WORK_DIR/source-c.out" && ! -s "$WORK_DIR/direct-c.out" ]] ||
    c_fail "C substitution leaked artifact text to stdout"
cmp -s "$C_SOURCE" "$C_DIRECT" ||
    c_fail "source-C and direct-MIR C did not consume one projection"
! grep -Fq 'scalar-program-route' "$WORK_DIR/direct-c.err" ||
    c_fail "nested intent C re-entered the scalar route"

pgy_selfhost_select_emitted_c_compile_profile ||
    c_fail "emitted C compile profile is invalid"
c_command=("$CC" -x c -std=c11 -Wall -Wextra -Werror)
c_command+=("${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[@]}")
if pgy_selfhost_emitted_c_uses_runtime_headers "$C_DIRECT"; then
    case "$(uname -s 2>/dev/null)" in
        Linux|*BSD|SunOS)
            c_command+=(-D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE)
            ;;
        Darwin)
            c_command+=(-D_DARWIN_C_SOURCE -D_XOPEN_SOURCE=700)
            ;;
    esac
    c_command+=(-DPGY_ZONE_THREADSAFE \
        "-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
fi
c_command+=("$C_DIRECT" -lm -o "$WORK_DIR/nested-c-program$suffix")
"${c_command[@]}" >"$WORK_DIR/c.compile.out" \
    2>"$WORK_DIR/c.compile.err" || {
        cat "$WORK_DIR/c.compile.err" >&2
        c_fail "shared-plan C artifact was not warning-clean"
    }
"$WORK_DIR/nested-c-program$suffix" | tr -d '\r' >"$WORK_DIR/c.run"
cmp -s "$WORK_DIR/c.run" "$WORK_DIR/expected.run" ||
    c_fail "shared-plan C execution differs from the exact nine-line contract"

for negative in missing-inner-priority priority-graph-drift \
    duplicate-source-identity method-owner-crosswire action-name-crosswire \
    observability-id-crosswire; do
    for route in source direct; do
        artifact_rel="$WORK_REL/$negative.$route.c"
        artifact="$ROOT_DIR/$artifact_rel"
        rm -f "$artifact"
        set +e
        if [[ "$route" == source ]]; then
            (cd "$ROOT_DIR" && "$DRIVER" --mir-json \
                "$WORK_REL/$negative.mir.json" -o "$artifact_rel") \
                >"$WORK_DIR/$negative.$route.out" \
                2>"$WORK_DIR/$negative.$route.err"
        else
            (cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend=c \
                "$WORK_REL/$negative.mir.json" -o "$artifact_rel") \
                >"$WORK_DIR/$negative.$route.out" \
                2>"$WORK_DIR/$negative.$route.err"
        fi
        rc=$?
        set -e
        [[ "$rc" -ne 0 && ! -e "$artifact" ]] ||
            c_fail "$negative $route route published an artifact"
        case "$negative" in
            missing-inner-priority) receipt="direct MIR nested intent carrier spine is invalid" ;;
            priority-graph-drift) receipt="direct MIR nested intent priority binding is missing" ;;
            duplicate-source-identity) receipt="MIR machine-layer facts are missing or invalid" ;;
            method-owner-crosswire) receipt="MIR machine-layer facts are missing or invalid" ;;
            action-name-crosswire) receipt="direct MIR nested intent callable identity is stale" ;;
            observability-id-crosswire) receipt="direct MIR nested intent executable graph is invalid" ;;
        esac
        grep -Fq "$receipt" "$WORK_DIR/$negative.$route.out" \
            "$WORK_DIR/$negative.$route.err" ||
            c_fail "$negative $route route lost its owned diagnostic"
        ! grep -Fq 'scalar-program-route' "$WORK_DIR/$negative.$route.out" \
            "$WORK_DIR/$negative.$route.err" ||
            c_fail "$negative $route route fell through to scalar"
    done
done

"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])
compiler = root / "src/self_hosted/compiler"
emitter = (compiler / "direct_mir_nested_intent_program_c_emission_owner.pgy").read_text()
projection = (compiler / "direct_mir_nested_intent_program_projection_owner.pgy").read_text()
driver = (compiler / "driver_rung2_owner.pgy").read_text()
multi = (compiler / "direct_mir_multi_routine_projection_owner.pgy").read_text()
for forbidden in ("MirMachineLayerAdmittedJsonInput", "source_json", "Json", "Ast",
                  "DriverRung2IntentTreeEmissionOrDie", "scalar-program-route"):
    assert forbidden not in emitter, f"C emitter reopened {forbidden}"
assert "if !is_llvm { return None; }" not in projection
route = projection.index("DirectMirNestedIntentProgramRouteFactFromAdmitted")
plan = projection.index("DirectMirNestedIntentProgramPlanFromAdmitted")
emit_c = projection.index("DirectMirNestedIntentProgramEmitC")
emit_llvm = projection.index("DirectMirNestedIntentProgramEmitLlvm")
assert route < plan < emit_c and route < plan < emit_llvm
claimed = "DriverRung2NestedIntentCSubstitutionIfClaimed("
assert driver.index(claimed) < driver.index("DriverRung2IntentTreeEmissionOrDie")
shared = "CompileAdmittedDirectMirNestedIntentProgramForTargetIfClaimed"
assert multi.index(shared) < multi.index("DirectMirScalarProgramRouteAdmissionFromAdmitted")
PY

echo "[$C_LABEL] LLVM parity plus byte-identical source/direct C, exact runtime, and twelve no-artifact negatives: PASS"

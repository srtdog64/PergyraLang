#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-parity:driver-execution-action-abi"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[$LABEL] missing compiler binary: $PGY" >&2; exit 1; }
pgy_reject_wsl_windows_pgy_parity_mix "$LABEL" "$PGY"

SOURCE="$ROOT_DIR/tests/self_hosted/fixtures/driver_execution_action_abi_probe.pgy"
C_AUTHORITY_SYNC="$ROOT_DIR/src/codegen/transpiler_projection_sync.c"
C_AUTHORITY_CALLER="$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
LLVM_AUTHORITY_SYNC="$ROOT_DIR/src/codegen/llvm_expr_call_methods_world_effect_sync.c"
LLVM_AUTHORITY_CALLER="$ROOT_DIR/src/codegen/llvm_member_call_emit.c"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver_execution_action_abi}"
EXPECTED_OUTPUT=$'ok\nartifact-written\n17\ntrue\nDriverExecutionActionAbiZone\nprobe'
EXPECTED_FILE_CONTENT="driver-action-abi"
mkdir -p "$BUILD_DIR"

for required_file in "$SOURCE" "$C_AUTHORITY_SYNC" "$C_AUTHORITY_CALLER" \
    "$LLVM_AUTHORITY_SYNC" "$LLVM_AUTHORITY_CALLER"; do
    [[ -f "$required_file" ]] || {
        echo "[$LABEL] missing authority owner/caller: $required_file" >&2
        exit 1
    }
done

require_source_term() {
    local term="$1"
    grep -Fq -- "$term" "$SOURCE" || {
        echo "[$LABEL] fixture no longer proves required ABI term: $term" >&2
        exit 1
    }
}

require_source_term "enum DriverExecutionActionAbiStage"
require_source_term "struct DriverExecutionActionAbiRequest"
require_source_term "struct DriverExecutionActionAbiResult"
require_source_term "let stage: DriverExecutionActionAbiStage;"
require_source_term "subject DriverExecutionActionAbiProbe"
require_source_term "request: DriverExecutionActionAbiRequest"
require_source_term ") -> DriverExecutionActionAbiResult"
require_source_term "within DriverExecutionActionAbiZone"
require_source_term "authorized by self"
require_source_term "zone DriverExecutionActionAbiZone"
require_source_term "subject slot other: DriverExecutionActionAbiOther"
require_source_term "authority probe"
require_source_term "world DriverExecutionActionAbiWorld"
require_source_term "let compiler_world = DriverExecutionActionAbiWorld("
require_source_term "WriteFile(request.output_path, request.payload);"
require_source_term "let observed: String = ReadFile(request.output_path);"
require_source_term "compiler_world.Execute(request);"
require_source_term "pgy_zone_authority_last_ok_rt_export()"
require_source_term "pgy_zone_authority_last_zone_rt_export()"
require_source_term "pgy_zone_authority_last_participant_rt_export()"

require_authority_owner_term() {
    local owner="$1"
    local term="$2"
    grep -Fq -- "$term" "$owner" || {
        echo "[$LABEL] world-action authority helper lost fail-closed term: $term" >&2
        exit 1
    }
}

for term in \
    'transpiler_mir_decl_method_authorized_by_count(method_meta) != 1' \
    'transpiler_set_mir_inventory_missing(ctx,' \
    'MIR-only C path cannot bind multiple authorities for a world-embedded action' \
    "MIR-only C path cannot bind named authority '%s' for a world-embedded action" \
    'MIR-only C path cannot bind indirect world action authority to exact zone/subject slots' \
    'if (*check_out == NULL)' \
    'MIR-only C path failed to materialize the world action authority check'; do
    require_authority_owner_term "$C_AUTHORITY_SYNC" "$term"
done
for term in \
    'llvm_mir_decl_method_authorized_by_count(method_meta) != 1' \
    'llvm_set_mir_inventory_missing(ctx,' \
    'MIR-only LLVM path cannot bind multiple authorities for a world-embedded action' \
    "MIR-only LLVM path cannot bind named authority '%s' for a world-embedded action" \
    'MIR-only LLVM path cannot bind indirect world action authority to exact zone/subject slots'; do
    require_authority_owner_term "$LLVM_AUTHORITY_SYNC" "$term"
done

for owner_and_term in \
    "$C_AUTHORITY_SYNC|MIR-only C path cannot bind multiple authorities for a world-embedded action" \
    "$C_AUTHORITY_SYNC|MIR-only C path cannot bind named authority '%s' for a world-embedded action" \
    "$C_AUTHORITY_SYNC|MIR-only C path cannot bind indirect world action authority to exact zone/subject slots" \
    "$LLVM_AUTHORITY_SYNC|MIR-only LLVM path cannot bind multiple authorities for a world-embedded action" \
    "$LLVM_AUTHORITY_SYNC|MIR-only LLVM path cannot bind named authority '%s' for a world-embedded action" \
    "$LLVM_AUTHORITY_SYNC|MIR-only LLVM path cannot bind indirect world action authority to exact zone/subject slots"; do
    owner="${owner_and_term%%|*}"
    term="${owner_and_term#*|}"
    [[ "$(grep -F -c -- "$term" "$owner")" -eq 1 ]] || {
        echo "[$LABEL] authority rejection diagnostic must occur exactly once: $term" >&2
        exit 1
    }
done

awk '
    /emit_world_embedded_action_authority_check\(/ {
        calls++
        pending = 6
        next
    }
    pending > 0 {
        if (/ctx->backend_error != NULL/) {
            checked++
            pending = 0
            next
        }
        pending--
    }
    END { exit !(calls == 1 && checked == calls) }
' "$C_AUTHORITY_CALLER" || {
    echo "[$LABEL] C authority caller can continue after an explicit helper error" >&2
    exit 1
}
awk '
    /llvm_emit_world_embedded_action_authority_check\(/ {
        calls++
        pending = 4
        next
    }
    pending > 0 {
        if (/if \(ctx->has_error\)/) {
            checked++
            pending = 0
            next
        }
        pending--
    }
    END { exit !(calls > 0 && checked == calls) }
' "$LLVM_AUTHORITY_CALLER" || {
    echo "[$LABEL] LLVM authority caller can continue after an explicit helper error" >&2
    exit 1
}

run_backend() {
    local backend="$1"
    local bin="$BUILD_DIR/driver_execution_action_abi_${backend}.exe"
    local compile_log="$BUILD_DIR/driver_execution_action_abi_${backend}.compile.log"
    local stdout_file="$BUILD_DIR/driver_execution_action_abi_${backend}.out"
    local stderr_file="$BUILD_DIR/driver_execution_action_abi_${backend}.err"
    local artifact="$BUILD_DIR/${backend}.artifact.txt"
    local artifact_rel="${artifact#"$ROOT_DIR"/}"
    local observed=""

    if [[ "$artifact_rel" == "$artifact" ]]; then
        echo "[$LABEL] build directory must remain under the repository root" >&2
        return 1
    fi
    rm -f "$bin" "$stdout_file" "$stderr_file" "$artifact"
    if ! (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$SOURCE")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$bin")" \
        >"$compile_log" 2>&1); then
        echo "[$LABEL] $backend compile failed" >&2
        cat "$compile_log" >&2
        return 1
    fi
    pgy_require_runnable_binary_here "$LABEL:$backend" "$bin" || return 1

    if ! (cd "$ROOT_DIR" && "$bin" "$artifact_rel" >"$stdout_file" 2>"$stderr_file"); then
        echo "[$LABEL] $backend runtime failed" >&2
        cat "$stdout_file" "$stderr_file" >&2
        return 1
    fi
    if [[ -s "$stderr_file" ]]; then
        echo "[$LABEL] $backend runtime wrote unexpected stderr" >&2
        cat "$stderr_file" >&2
        return 1
    fi

    observed="$(tr -d '\r' < "$stdout_file")"
    if [[ "$observed" != "$EXPECTED_OUTPUT" ]]; then
        echo "[$LABEL] $backend returned the wrong action result/stage" >&2
        printf 'expected:\n%s\nactual:\n%s\n' "$EXPECTED_OUTPUT" "$observed" >&2
        return 1
    fi
    if [[ ! -f "$artifact" ]]; then
        echo "[$LABEL] $backend action did not create its artifact" >&2
        return 1
    fi
    if [[ "$(cat "$artifact")" != "$EXPECTED_FILE_CONTENT" ]]; then
        echo "[$LABEL] $backend action artifact content mismatch" >&2
        return 1
    fi
}

run_backend c
run_backend llvm

run_authority_negative() {
    local variant="$1"
    local backend="$2"
    local source="$BUILD_DIR/driver_execution_action_abi_${variant}.pgy"
    local bin="$BUILD_DIR/driver_execution_action_abi_${variant}_${backend}.exe"
    local log="$BUILD_DIR/driver_execution_action_abi_${variant}_${backend}.log"
    local artifact="$BUILD_DIR/${variant}_${backend}.artifact.txt"

    rm -f "$source" "$bin" "$log" "$artifact"
    if [[ "$variant" == "missing_authority" ]]; then
        sed '/^[[:space:]]*authority probe[[:space:]]*$/d' "$SOURCE" >"$source"
    else
        sed 's/authority probe/authority other/' "$SOURCE" >"$source"
    fi
    if (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$source")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$bin")" \
        >"$log" 2>&1); then
        echo "[$LABEL] $variant/$backend unexpectedly compiled" >&2
        return 1
    fi
    grep -Fq -- "declares no matching authority" "$log" || {
        echo "[$LABEL] $variant/$backend lost the authority diagnostic" >&2
        cat "$log" >&2
        return 1
    }
    if [[ -e "$bin" || -e "$artifact" ]]; then
        echo "[$LABEL] $variant/$backend left an executable or artifact" >&2
        return 1
    fi
}

for backend in c llvm; do
    run_authority_negative missing_authority "$backend"
    run_authority_negative wrong_authority "$backend"
done

run_unsupported_world_authority_negative() {
    local variant="$1"
    local backend="$2"
    local backend_name="C"
    local source="$BUILD_DIR/driver_execution_action_abi_${variant}_world_authority.pgy"
    local bin="$BUILD_DIR/driver_execution_action_abi_${variant}_world_authority_${backend}.exe"
    local log="$BUILD_DIR/driver_execution_action_abi_${variant}_world_authority_${backend}.log"
    local artifact="$BUILD_DIR/${variant}_world_authority_${backend}.artifact.txt"
    local diagnostic

    [[ "$backend" == "llvm" ]] && backend_name="LLVM"
    case "$variant" in
        named)
            diagnostic="MIR-only $backend_name path cannot bind named authority 'other' for a world-embedded action"
            ;;
        multiple)
            diagnostic="MIR-only $backend_name path cannot bind multiple authorities for a world-embedded action"
            ;;
        indirect)
            diagnostic="MIR-only $backend_name path cannot bind indirect world action authority to exact zone/subject slots"
            ;;
        *)
            echo "[$LABEL] unknown unsupported authority shape: $variant" >&2
            return 1
            ;;
    esac
    rm -f "$source" "$bin" "$log" "$artifact"
    awk -v variant="$variant" '
        variant == "indirect" &&
            /request: DriverExecutionActionAbiRequest/ {
            request_parameter_count++
            if (request_parameter_count == 2) {
                print "        ref indirect_probe: DriverExecutionActionAbiProbe,"
                indirect_parameter_added = 1
            }
        }
        variant != "indirect" && !parameter_added &&
            /request: DriverExecutionActionAbiRequest/ {
            print "        other: DriverExecutionActionAbiProbe,"
            parameter_added = 1
        }
        variant != "indirect" && !authority_replaced &&
            /authorized by self/ {
            if (variant == "named") {
                sub(/authorized by self/, "authorized by other")
            } else {
                sub(/authorized by self/, "authorized by self, other")
            }
            authority_replaced = 1
        }
        variant != "indirect" &&
            /return self\.execution\.probe\.Execute\(request\);/ {
            sub(/Execute\(request\)/,
                "Execute(self.execution.probe, request)")
            call_replaced = 1
        }
        variant == "indirect" &&
            /return self\.execution\.probe\.Execute\(request\);/ {
            print "        return indirect_probe.Execute(request);"
            indirect_call_replaced = 1
            next
        }
        variant == "indirect" &&
            /compiler_world\.Execute\(request\);/ {
            sub(/Execute\(request\)/,
                "Execute(compiler_world.execution.probe, request)")
            indirect_world_call_replaced = 1
            print
            next
        }
        { print }
        END {
            if (variant == "indirect") {
                if (!indirect_parameter_added || !indirect_call_replaced ||
                    !indirect_world_call_replaced) exit 1
            } else if (!parameter_added || !authority_replaced ||
                       !call_replaced) {
                exit 1
            }
        }
    ' "$SOURCE" >"$source"

    if (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$source")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$bin")" \
        >"$log" 2>&1); then
        echo "[$LABEL] $variant world authority/$backend unexpectedly compiled" >&2
        return 1
    fi
    grep -Fq -- "$diagnostic" "$log" || {
        echo "[$LABEL] $variant world authority/$backend was not rejected by the backend helper" >&2
        cat "$log" >&2
        return 1
    }
    if [[ -e "$bin" || -e "$artifact" ]]; then
        echo "[$LABEL] $variant world authority/$backend left an executable or artifact" >&2
        return 1
    fi
}

for backend in c llvm; do
    for variant in named multiple indirect; do
        run_unsupported_world_authority_negative "$variant" "$backend"
    done
done

if ! cmp -s "$BUILD_DIR/c.artifact.txt" "$BUILD_DIR/llvm.artifact.txt"; then
    echo "[$LABEL] C/LLVM action artifacts differ" >&2
    exit 1
fi
if ! cmp -s \
    "$BUILD_DIR/driver_execution_action_abi_c.out" \
    "$BUILD_DIR/driver_execution_action_abi_llvm.out"; then
    echo "[$LABEL] C/LLVM action result/stage output differs" >&2
    exit 1
fi

echo "[$LABEL] subject/action/zone/world aggregate ABI, authority snapshot, capabilities, and WriteFile parity ok"

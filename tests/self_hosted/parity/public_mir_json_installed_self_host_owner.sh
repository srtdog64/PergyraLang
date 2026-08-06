#!/usr/bin/env bash
# Public source-to-MIR stdout is owned by the installed Pergyra-built sibling.
# Native C MIR remains reachable only through the explicit frozen test oracle.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
WORK_REL=".tmp/self_hosted/public_mir_json_installed"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE="tests/self_hosted/parity/fixture/intent_typed_outcome_execution.pgy"
LAUNCHER_OWNER="$ROOT_DIR/src/pgy_driver.c"
SELECTION_OWNER="$ROOT_DIR/src/compiler/driver_self_host_selection_owner.c"
SIBLING_OWNER="$ROOT_DIR/src/compiler/self_host_driver.c"

fail() {
    echo "[self-host-public-mir-json] $*" >&2
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

PGY="$(cd "$(dirname "$PGY")" && pwd -P)/$(basename "$PGY")"
SELF_DRIVER="$(cd "$(dirname "$SELF_DRIVER")" && pwd -P)/$(basename "$SELF_DRIVER")"
installed_name="pgy-self-driver"
[[ "$PGY" == *.exe ]] && installed_name="pgy-self-driver.exe"
[[ "$SELF_DRIVER" == "$(dirname "$PGY")/$installed_name" ]] ||
    fail "self-host driver is not installed beside the public launcher"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/{direct,public,native,native.canonical,self.canonical}.json \
    "$WORK_DIR"/{missing,unsupported,mixed,rejected}.{out,err}

(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-verified "$SOURCE") \
    >"$WORK_DIR/direct.json"
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN && "$PGY" --mir-json "$SOURCE") \
    >"$WORK_DIR/public.json"
cmp -s "$WORK_DIR/direct.json" "$WORK_DIR/public.json" ||
    fail "public --mir-json differs from the installed self-host producer"
grep -Fq '"schema":"pgy.mir.v1"' "$WORK_DIR/public.json" ||
    fail "public self-host producer did not emit pgy.mir.v1"

(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle "$SOURCE") \
    >"$WORK_DIR/native.json" 2>"$WORK_DIR/native.err"
(cd "$ROOT_DIR" && "$SELF_DRIVER" --canonicalize-oracle-mir-json \
    "$WORK_REL/native.json") >"$WORK_DIR/native.canonical.json"
(cd "$ROOT_DIR" && "$SELF_DRIVER" --canonicalize-mir-json \
    "$WORK_REL/public.json") >"$WORK_DIR/self.canonical.json"
cmp -s "$WORK_DIR/native.canonical.json" "$WORK_DIR/self.canonical.json" ||
    fail "frozen native and installed self-host MIR facts differ"

cat >"$WORK_DIR/rejected.pgy" <<'PGY'
func Main() -> Void {
    MissingSourceMirSurface();
}
PGY
set +e
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-verified \
    "$WORK_REL/rejected.pgy") >"$WORK_DIR/rejected.direct.out" \
    2>"$WORK_DIR/rejected.direct.err"
rejected_direct_rc=$?
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN && "$PGY" --mir-json \
    "$WORK_REL/rejected.pgy") >"$WORK_DIR/rejected.out" \
    2>"$WORK_DIR/rejected.err"
rejected_public_rc=$?
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$WORK_REL/missing-driver" \
    "$PGY" --mir-json "$SOURCE") >"$WORK_DIR/missing.out" \
    2>"$WORK_DIR/missing.err"
missing_rc=$?
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN && "$PGY" --mir-json "$SOURCE" \
    --runtime=none) >"$WORK_DIR/unsupported.out" 2>"$WORK_DIR/unsupported.err"
unsupported_rc=$?
(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle --mir-json \
    "$SOURCE") >"$WORK_DIR/mixed.out" 2>"$WORK_DIR/mixed.err"
mixed_rc=$?
set -e

[[ "$rejected_direct_rc" -ne 0 && "$rejected_public_rc" -eq "$rejected_direct_rc" ]] ||
    fail "rejected source status drifted between direct and public self-host paths"
tr -d '\r' <"$WORK_DIR/rejected.direct.err" >"$WORK_DIR/rejected.direct.norm"
tr -d '\r' <"$WORK_DIR/rejected.err" >"$WORK_DIR/rejected.norm"
cmp -s "$WORK_DIR/rejected.direct.norm" "$WORK_DIR/rejected.norm" ||
    fail "rejected source did not preserve the installed self-host diagnostic"
cmp -s "$WORK_DIR/rejected.direct.out" "$WORK_DIR/rejected.out" ||
    fail "rejected source stdout drifted between direct and public self-host paths"
grep -Fq '"schema":"pgy.mir.v1"' "$WORK_DIR/rejected.out" &&
    fail "rejected source emitted public MIR"
[[ "$missing_rc" -ne 0 && ! -s "$WORK_DIR/missing.out" ]] ||
    fail "missing sibling silently entered native MIR production"
grep -Fq "self-host driver is unavailable" "$WORK_DIR/missing.err" ||
    fail "missing sibling did not fail explicitly"
[[ "$unsupported_rc" -ne 0 && ! -s "$WORK_DIR/unsupported.out" ]] ||
    fail "unsupported public options silently entered native MIR production"
grep -Fq "outside the installed self-host driver contract" \
    "$WORK_DIR/unsupported.err" || fail "unsupported options lost selector diagnostic"
[[ "$mixed_rc" -ne 0 && ! -s "$WORK_DIR/mixed.out" ]] ||
    fail "test oracle and public mode were admitted together"

require_text "$LAUNCHER_OWNER" 'if (flags.test_native_mir_json_oracle) {'
require_text "$LAUNCHER_OWNER" 'if (flags.dump_mir_json) {'
require_text "$LAUNCHER_OWNER" 'return driver_run_self_host_mir_json(argv[0], flags.source_path);'
require_text "$LAUNCHER_OWNER" 'return driver_run_pipeline(&flags);'
require_text "$SELECTION_OWNER" 'driver_self_host_mir_json_request_supported('
require_text "$SELECTION_OWNER" '&& !flags->test_native_mir_json_oracle'
require_text "$SIBLING_OWNER" 'args[0] = (char *)"--emit-mir-json-verified";'
require_text "$SIBLING_OWNER" 'return driver_run_self_host_command(launcher_path, 2, args);'
# Re-armed 2 -> 3: the third dispatch is the declared --native-pipeline /
# PGY_NATIVE_PIPELINE opt-out (docs/152), the single decision point that lets
# native-subject harnesses decline delegation. Any fourth site is a leak.
[[ "$(grep -F -c 'return driver_run_pipeline(&flags);' "$LAUNCHER_OWNER")" -eq 3 ]] ||
    fail "native pipeline reachability escaped test oracle, declared opt-out, and final non-MIR dispatch"
grep -Fq 'driver_run_pipeline(' "$SIBLING_OWNER" &&
    fail "installed sibling launcher regained a native pipeline fallback"
unseparated_oracles="$(find "$ROOT_DIR/tests" -type f -name '*.sh' \
    ! -name 'public_mir_json_installed_self_host_owner.sh' \
    ! -name 'public_llvm_ir_installed_self_host_owner.sh' -exec awk '
        /"\$PGY"/ { pgy_window = 3; self_mode = /--self-driver/ }
        pgy_window > 0 && /--mir-json/ && !/--test-native-mir-json-oracle/ && !self_mode {
            print FILENAME ":" FNR
        }
        pgy_window > 0 { pgy_window-- }
    ' {} +)"
[[ -z "$unseparated_oracles" ]] ||
    fail "a test reused public --mir-json as an unseparated native oracle"

echo "[self-host-public-mir-json] installed Pergyra producer owns public source-to-MIR; frozen native oracle remains independent"

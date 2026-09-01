#!/usr/bin/env bash
# Profile-neutral verified replay of the installed immutable machine companion.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
WORK_REL=".tmp/self_hosted/public_machine_manifest_installed"
WORK_DIR="$ROOT_DIR/$WORK_REL"

fail() {
    echo "[self-host-public-machine-manifest] $*" >&2
    exit 1
}

require_text() {
    grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"
}

companion_for() {
    case "$1" in
        *.exe) printf '%s\n' "${1%.exe}.machine-layer-manifest.json" ;;
        *) printf '%s\n' "${1}.machine-layer-manifest.json" ;;
    esac
}

PGY="$(pgy_select_optional_exe_binary "$PGY")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "$SELF_DRIVER")"
[[ -x "$PGY" ]] || fail "missing public pgy launcher: $PGY"
[[ -x "$SELF_DRIVER" ]] || fail "missing installed self-host driver: $SELF_DRIVER"
PGY="$(cd "$(dirname "$PGY")" && pwd -P)/$(basename "$PGY")"
SELF_DRIVER="$(cd "$(dirname "$SELF_DRIVER")" && pwd -P)/$(basename "$SELF_DRIVER")"
COMPANION="$(companion_for "$SELF_DRIVER")"
[[ -f "$COMPANION" ]] || fail "missing installed machine manifest companion"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

(cd "$ROOT_DIR" && PGY_IO_ALLOW_ABSOLUTE=1 "$SELF_DRIVER" \
    --emit-machine-manifest-verified "$COMPANION") \
    >"$WORK_DIR/direct.out" 2>"$WORK_DIR/direct.err" ||
    fail "installed driver rejected its packaged companion"
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE && \
    "$PGY" --machine-manifest-json) \
    >"$WORK_DIR/public.out" 2>"$WORK_DIR/public.err"
(cd "$WORK_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE && \
    "$PGY" --machine-manifest-json --opt=dev) \
    >"$WORK_DIR/public-cwd.out" 2>"$WORK_DIR/public-cwd.err"
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE && \
    "$PGY" --machine-manifest-json --opt=dev) \
    >"$WORK_DIR/public-dev.out" 2>"$WORK_DIR/public-dev.err"
(cd "$ROOT_DIR" && "$PGY" --native-pipeline --machine-manifest-json --opt=dev) \
    >"$WORK_DIR/native.out" 2>"$WORK_DIR/native.err"
cmp -s "$COMPANION" "$WORK_DIR/direct.out" ||
    fail "installed driver changed the verified companion bytes"
cmp -s "$WORK_DIR/direct.out" "$WORK_DIR/public.out" ||
    fail "public manifest differs from installed self-host output"
cmp -s "$WORK_DIR/public.out" "$WORK_DIR/public-dev.out" ||
    fail "public manifest changed bytes under --opt=dev"
cmp -s "$WORK_DIR/direct.out" "$WORK_DIR/public-cwd.out" ||
    fail "public manifest depends on the caller working directory"
cmp -s "$WORK_DIR/direct.out" "$WORK_DIR/native.out" ||
    fail "installed manifest differs from the native owner oracle"

suffix=""
[[ "$SELF_DRIVER" == *.exe ]] && suffix=".exe"
missing_driver="$WORK_DIR/missing-driver${suffix}"
invalid_driver="$WORK_DIR/invalid-driver${suffix}"
cp "$SELF_DRIVER" "$missing_driver"
cp "$SELF_DRIVER" "$invalid_driver"
chmod +x "$missing_driver" "$invalid_driver"
invalid_companion="$(companion_for "$invalid_driver")"
printf '{}\n' >"$invalid_companion"

set +e
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$missing_driver" \
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" --machine-manifest-json --opt=dev) \
    >"$WORK_DIR/missing.out" 2>"$WORK_DIR/missing.err"
missing_rc=$?
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$invalid_driver" \
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" --machine-manifest-json --opt=dev) \
    >"$WORK_DIR/invalid.out" 2>"$WORK_DIR/invalid.err"
invalid_rc=$?
(cd "$ROOT_DIR" && "$PGY" --machine-manifest-json --opt=dev --verbose) \
    >"$WORK_DIR/options.out" 2>"$WORK_DIR/options.err"
options_rc=$?
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-machine-manifest-verified) \
    >"$WORK_DIR/arity.out" 2>"$WORK_DIR/arity.err"
arity_rc=$?
set -e

[[ "$missing_rc" -ne 0 ]] || fail "missing companion was accepted"
grep -Fq "machine manifest companion is unavailable" "$WORK_DIR/missing.err" ||
    fail "missing companion lost its installed-boundary diagnostic"
[[ ! -s "$WORK_DIR/missing.out" ]] || fail "missing companion emitted output"
[[ "$invalid_rc" -ne 0 ]] || fail "invalid companion was accepted"
! grep -Fq '"schema":"pgy.machine-layer.declaration.v1"' \
    "$WORK_DIR/invalid.out" || fail "invalid companion emitted manifest bytes"
grep -Fq "installed machine declaration artifact is invalid" \
    "$WORK_DIR/invalid.out" "$WORK_DIR/invalid.err" ||
    fail "invalid companion lost its typed diagnostic"
[[ "$options_rc" -ne 0 ]] || fail "unsupported manifest options were accepted"
grep -Fq -- "--machine-manifest-json options are outside" \
    "$WORK_DIR/options.err" || fail "unsupported options lost selector diagnostic"
[[ "$arity_rc" -ne 0 ]] || fail "installed mode accepted a missing artifact path"
grep -Fq "installed machine manifest mode requires one artifact path" \
    "$WORK_DIR/arity.out" "$WORK_DIR/arity.err" ||
    fail "installed arity lost its typed diagnostic"
! grep -Fq "[pipeline timing]" "$WORK_DIR/missing.err" \
    "$WORK_DIR/invalid.err" || fail "manifest failure retried the native pipeline"

require_text "$ROOT_DIR/src/pgy_driver.c" \
    'driver_self_host_machine_manifest_request_supported(&flags)'
require_text "$ROOT_DIR/src/pgy_driver.c" \
    'return driver_write_self_host_machine_manifest(argv[0]);'
require_text "$ROOT_DIR/src/compiler/self_host_machine_manifest_artifact_owner.c" \
    'child_argv[1] = "--emit-machine-manifest-verified";'
require_text "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy" \
    'DriverCliMachineManifestStdout(String)'
! grep -Fq 'DriverCliMachineManifestStdout(String,' \
    "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy" ||
    fail "machine-manifest request gained a second policy input"
selection_body="$(sed -n '/driver_self_host_machine_manifest_request_supported(/,/^}/p' \
    "$ROOT_DIR/src/compiler/driver_self_host_selection_owner.c")"
! grep -Fq 'opt_profile' <<<"$selection_body" ||
    fail "C admission still assigns optimization semantics to the manifest"
require_text "$ROOT_DIR/src/self_hosted/compiler/machine_layer_declaration_consumer.pgy" \
    'SelfHostMachineLayerDeclarationArtifactPayloadFromPathVerified('
require_text "$ROOT_DIR/src/self_hosted/compiler/machine_layer_declaration_consumer.pgy" \
    'CharCode(payload, length, length - 2) == 13'
if grep -Fq 'driver_run_pipeline(' \
    "$ROOT_DIR/src/compiler/self_host_machine_manifest_artifact_owner.c"; then
    fail "machine manifest adapter regained a native fallback"
fi

echo "[self-host-public-machine-manifest] installed verified companion owns public output and fails closed"

#!/usr/bin/env bash
# Package compilation consumes the manifest entry/backend once, then delegates
# to the same installed MIR/C/LLVM artifact owners as the public source CLI.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-cc}"
WORK_DIR="$ROOT_DIR/.tmp/self_hosted/package_commands_installed"

fail() {
    echo "[self-host-package-commands] $*" >&2
    exit 1
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
command -v clang >/dev/null 2>&1 || fail "missing LLVM IR-capable clang"

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

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR/real" "$WORK_DIR/counting-install" \
    "$WORK_DIR/counting-package" "$WORK_DIR/missing-install"

# Real installed artifacts prove the package entry is not merely routed to a
# permissive shim. The existing default C/LLVM gates own broader language
# parity; this leg pins the package command boundary itself.
(
    cd "$WORK_DIR/real"
    unset PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN
    "$PGY" init installed-package >init.out 2>init.err
    "$PGY" package >package.out 2>package.err
    "$PGY" check >check.out 2>check.err
    "$PGY" build >build.out 2>build.err
    "$PGY" run >run.out 2>run.err
)
grep -Fq "pgy package-check: main.pgy ok" "$WORK_DIR/real/package.out" ||
    fail "real installed package verification did not finish"
grep -Fq "pgy check: main.pgy ok" "$WORK_DIR/real/check.out" ||
    fail "real installed package check did not finish"
grep -Fq "pgy build: main.pgy ok" "$WORK_DIR/real/build.out" ||
    fail "real installed package build did not finish"
grep -Fq "Hello, installed-package!" "$WORK_DIR/real/run.out" ||
    fail "real installed package binary produced the wrong output"
grep -Fq "pgy run: main.pgy ok" "$WORK_DIR/real/run.out" ||
    fail "real installed package run did not finish"
cp "$WORK_DIR/real/pgy.lock" "$WORK_DIR/real/pgy.lock.before-invalid"
cp "$WORK_DIR/real/main.pgy" "$WORK_DIR/real/main.valid.pgy"
printf 'func Main( -> Void {\n' >"$WORK_DIR/real/main.pgy"
set +e
(
    cd "$WORK_DIR/real"
    unset PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" package
) >"$WORK_DIR/real/invalid.out" 2>"$WORK_DIR/real/invalid.err"
invalid_rc=$?
set -e
[[ "$invalid_rc" -ne 0 ]] || fail "invalid package source was accepted"
grep -Fq "self-host MIR producer failed with code" \
    "$WORK_DIR/real/invalid.err" ||
    fail "invalid package source did not fail at the installed MIR boundary"
cmp -s "$WORK_DIR/real/pgy.lock.before-invalid" "$WORK_DIR/real/pgy.lock" ||
    fail "failed installed package verification published a new lock"
! grep -Fq "[pipeline timing]" "$WORK_DIR/real/invalid.err" ||
    fail "invalid package source retried through the native pipeline"

cp "$PGY" "$WORK_DIR/counting-install/pgy$suffix"
"$CC" -std=c11 -Wall -Wextra -Werror \
    "$ROOT_DIR/tests/self_hosted/parity/fixture/counting_package_self_host_driver.c" \
    -o "$WORK_DIR/counting-install/$installed_name"
COUNT_FILE="$WORK_DIR/count.txt"
COUNT_FILE_FOR_DRIVER="$COUNT_FILE"
if [[ "$PGY" == *.exe ]]; then
    COUNT_FILE_FOR_DRIVER="$(pgy_path_for_compiler "$PGY" "$COUNT_FILE")"
fi
COUNTING_PGY="$WORK_DIR/counting-install/pgy$suffix"
(
    cd "$WORK_DIR/counting-package"
    unset PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN
    "$COUNTING_PGY" init route-package >/dev/null
    PGY_DEBUG_PIPELINE_TIMING=1 \
    PGY_SELF_DRIVER_COUNT_FILE="$COUNT_FILE_FOR_DRIVER" \
        "$COUNTING_PGY" package >package.out 2>package.err
    PGY_DEBUG_PIPELINE_TIMING=1 \
    PGY_SELF_DRIVER_COUNT_FILE="$COUNT_FILE_FOR_DRIVER" \
        "$COUNTING_PGY" check >check.out 2>check.err
    PGY_DEBUG_PIPELINE_TIMING=1 \
    PGY_SELF_DRIVER_COUNT_FILE="$COUNT_FILE_FOR_DRIVER" \
        "$COUNTING_PGY" build >build.out 2>build.err
    PGY_DEBUG_PIPELINE_TIMING=1 \
    PGY_SELF_DRIVER_COUNT_FILE="$COUNT_FILE_FOR_DRIVER" \
        "$COUNTING_PGY" run >run.out 2>run.err
    PGY_DEBUG_PIPELINE_TIMING=1 \
    PGY_SELF_DRIVER_COUNT_FILE="$COUNT_FILE_FOR_DRIVER" \
        "$COUNTING_PGY" test >test.out 2>test.err
    PGY_DEBUG_PIPELINE_TIMING=1 \
    PGY_SELF_DRIVER_COUNT_FILE="$COUNT_FILE_FOR_DRIVER" \
        "$COUNTING_PGY" lint >lint.out 2>lint.err
    PGY_DEBUG_PIPELINE_TIMING=1 \
    PGY_SELF_DRIVER_COUNT_FILE="$COUNT_FILE_FOR_DRIVER" \
        "$COUNTING_PGY" prove >prove.out 2>prove.err
)
printf 'mir\nmir\nc\nc\nc\nmir\nmir\n' >"$WORK_DIR/c.expected"
cmp -s "$WORK_DIR/c.expected" "$COUNT_FILE" ||
    fail "C package commands did not consume the expected installed artifacts"
grep -Fxq "package-self-host-shim" "$WORK_DIR/counting-package/run.out" ||
    fail "package run did not execute the installed C artifact"
grep -Fxq "package-self-host-shim" "$WORK_DIR/counting-package/test.out" ||
    fail "package test did not execute the installed C artifact"
! grep -Fq "[pipeline timing]" "$WORK_DIR/counting-package"/*.err ||
    fail "default package command re-entered the native pipeline"

# Backend selection remains manifest-owned. Package verification consumes one
# MIR request; each binary target then consumes one compiler-purpose intent
# without a second C-owned MIR/backend pair.
sed -i.bak 's/backend = "c"/backend = "llvm"/' \
    "$WORK_DIR/counting-package/pgy.toml"
rm -f "$COUNT_FILE"
(
    cd "$WORK_DIR/counting-package"
    unset PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN
    PGY_SELF_DRIVER_COUNT_FILE="$COUNT_FILE_FOR_DRIVER" \
        "$COUNTING_PGY" package >llvm-package.out 2>llvm-package.err
    PGY_SELF_DRIVER_COUNT_FILE="$COUNT_FILE_FOR_DRIVER" \
        "$COUNTING_PGY" build >llvm-build.out 2>llvm-build.err
    PGY_SELF_DRIVER_COUNT_FILE="$COUNT_FILE_FOR_DRIVER" \
        "$COUNTING_PGY" run >llvm-run.out 2>llvm-run.err
)
printf 'mir\nintent\nintent\n' >"$WORK_DIR/llvm.expected"
cmp -s "$WORK_DIR/llvm.expected" "$COUNT_FILE" ||
    fail "LLVM package commands ignored the manifest backend owner"
grep -Fxq "package-self-host-shim" "$WORK_DIR/counting-package/llvm-run.out" ||
    fail "package run did not execute the installed LLVM artifact"

# Absence is a typed boundary, never permission to fall back to the C compiler.
cp "$PGY" "$WORK_DIR/missing-install/pgy$suffix"
set +e
(
    cd "$WORK_DIR/counting-package"
    unset PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN
    "$WORK_DIR/missing-install/pgy$suffix" check
) >"$WORK_DIR/missing.out" 2>"$WORK_DIR/missing.err"
missing_rc=$?
set -e
[[ "$missing_rc" -ne 0 ]] || fail "missing driver used a native package fallback"
grep -Fq "self-host driver is unavailable" "$WORK_DIR/missing.err" ||
    fail "missing package driver did not report the installed boundary"
! grep -Fq "[pipeline timing]" "$WORK_DIR/missing.err" ||
    fail "missing package driver entered the native compiler pipeline"

pkg_body="$(sed -n '/^pkg_run_entry(/,/^static int$/p' \
    "$ROOT_DIR/src/compiler/pkg.c")"
grep -Fq 'if (native_pipeline)' <<<"$pkg_body" ||
    fail "package native opt-out is not explicit"
[[ "$(grep -Fc 'driver_run_pipeline(' <<<"$pkg_body")" == "1" ]] ||
    fail "package entry has a hidden native pipeline path"
grep -Fq 'pkg_verify_entry_with_installed_self_host(' <<<"$pkg_body" ||
    fail "package checks do not consume the installed MIR owner"
grep -Fq 'c_runner_execute_installed_self_host_c(' <<<"$pkg_body" ||
    fail "package C binary does not consume the installed C runner"
grep -Fq 'llvm_runner_execute_installed_self_host_llvm(' <<<"$pkg_body" ||
    fail "package LLVM binary does not consume the installed LLVM runner"
! grep -Fq 'getenv("PGY_NATIVE_PIPELINE")' "$ROOT_DIR/src/compiler/pkg.c" ||
    fail "package owner duplicated the launcher execution-lane decision"

llvm_owner="$(sed -n '/^driver_materialize_self_host_llvm_artifact(/,/^}/p' \
    "$ROOT_DIR/src/compiler/self_host_llvm_driver.c")"
[[ "$(grep -Fc 'pgy_exec_argv(intent_argv, verbose)' <<<"$llvm_owner")" == "1" ]] ||
    fail "LLVM materializer does not invoke one compiler intent"
grep -Fq 'intent_argv[1] = "--emit-source-llvm-ir-verified"' <<<"$llvm_owner" ||
    fail "LLVM materializer lost the canonical compiler-intent request"
! grep -Eq 'driver_materialize_self_host_mir_artifact|--mir-json-backend=llvm' \
    <<<"$llvm_owner" || fail "LLVM materializer regained two-step C orchestration"

echo "[self-host-package-commands] package commands consume installed MIR/C/LLVM artifacts and fail closed"

#!/usr/bin/env bash
# Actual imported MIR owners: row preservation and linear allocation traffic.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd -P)"
source "$ROOT/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT/bin/pgy}")"
pgy_require_runnable_binary_here mir-row-mutation-cost "$PGY"
cd "$ROOT"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/mir-row-mutation-cost.XXXXXX)"
FIXTURE=tests/self_hosted/fixtures/mir_row_mutation_cost_probe.pgy
OBSERVER=tests/self_hosted/parity/mir/mir_row_allocation_observer.c
CHECK=tests/self_hosted/parity/mir/mir_row_mutation_cost_check.py
sha256sum "$PGY" "$FIXTURE" "$OBSERVER" "$CHECK" "${BASH_SOURCE[0]}" >"$WORK/inputs.sha256"
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z |
    xargs -0 sha256sum >"$WORK/imports.sha256"
timeout 240 "$PGY" --native-pipeline "$FIXTURE" --emit-c -o "$WORK/program.c" \
    >"$WORK/emit.out" 2>"$WORK/emit.err"
timeout 45 "${PGY_SELFHOST_CC:-gcc}" -x c -std=gnu11 -O0 -fwrapv \
    -fno-strict-aliasing "$WORK/program.c" "$OBSERVER" \
    -Isrc -Isrc/runtime -pthread \
    -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc -o "$WORK/program.exe" \
    >"$WORK/build.out" 2>"$WORK/build.err"
timeout 5 "$WORK/program.exe" >"$WORK/256.out" 2>"$WORK/256.err"
timeout 5 "$WORK/program.exe" larger >"$WORK/512.out" 2>"$WORK/512.err"
python3 "$CHECK" "$WORK"
sha256sum --quiet -c "$WORK/inputs.sha256"
sha256sum --quiet -c "$WORK/imports.sha256"

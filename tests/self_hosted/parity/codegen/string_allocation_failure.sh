#!/usr/bin/env bash
# Exercise the Pergyra codegen seed directly: no direct-MIR route can bypass it.
set -euo pipefail
ulimit -c 0
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
CODEGEN="$(pgy_select_optional_exe_binary "${PGY_CODEGEN_BIN:-$ROOT_DIR/.tmp/self_hosted/codegen/bootstrap/gen2.exe}")"
CC="${PGY_SELFHOST_CC:-gcc}"
PYTHON="${PYTHON_BIN:-python3}"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/string-allocation.XXXXXX")"
cd "$ROOT_DIR"
fail() { echo "[string-allocation] $*; evidence=$WORK" >&2; exit 1; }
if ! "$CODEGEN" --source tests/self_hosted/fixtures/string_allocation_failure.pgy \
    >"$WORK/probe.raw.c" 2>"$WORK/emit.log"; then
    tail -n 25 "$WORK/emit.log" >&2; fail "the codegen seed refused the String program";
fi
tr -d '\r' <"$WORK/probe.raw.c" >"$WORK/probe.c"
"$PYTHON" - "$WORK" <<'PY'
from pathlib import Path
import re
import sys
work = Path(sys.argv[1])
source = (work / 'probe.c').read_text()
start = source.index('static inline char *pgy_str_alloc(size_t length) {')
end = source.index('\n}', start) + 2
body, count = re.subn(
    r'if\s*\(\s*result\s*==\s*NULL\s*\)\s*\{\s*PGY_RUNTIME_PANIC\([^;]+;\s*\}',
    '', source[start:end], count=1)
if count != 1:
    raise SystemExit('the emitted allocator mutation could not remove its NULL guard')
(work / 'unchecked.c').write_text(source[:start] + body + source[end:])
PY
cat >"$WORK/fault.c" <<'C'
#include <stddef.h>
#include <stdio.h>
void *pgy_test_string_malloc(size_t bytes) {
    (void)bytes;
    fputs("forced-string-allocation-failure\n", stderr);
    return NULL;
}
C
cat >"$WORK/extent.c" <<'C'
#define main pgy_string_probe_main
#include "probe.c"
#undef main
int main(int argc, char **argv) {
    if (argc != 2) return 1;
    switch (atoi(argv[1])) {
    case 0:
        if (pgy_str_extent_add((size_t)-2, 1) != (size_t)-1 ||
            pgy_str_extent_multiply((size_t)-1, 1) != (size_t)-1 ||
            pgy_str_extent_multiply(0, (size_t)-1) != 0) return 1;
        puts("string-extent-ok");
        return 0;
    case 1: (void)pgy_str_extent_add((size_t)-1, 1); break;
    case 2: (void)pgy_str_extent_multiply((size_t)-1, 2); break;
    case 3: (void)pgy_str_alloc((size_t)-1); break;
    default: return 1;
    }
    return 1;
}
C
flags=(-std=c11 -O0 -g -fwrapv -fno-strict-aliasing -Isrc -Isrc/runtime -pthread)
if ! "$CC" "${flags[@]}" "$WORK/probe.c" -lm -o "$WORK/normal.exe" \
    >"$WORK/normal.build.log" 2>&1 ||
   ! "$CC" "${flags[@]}" -Dmalloc=pgy_test_string_malloc -c "$WORK/probe.c" \
    -o "$WORK/probe.o" >"$WORK/fault.build.log" 2>&1 ||
   ! "$CC" "${flags[@]}" "$WORK/probe.o" "$WORK/fault.c" -lm -o "$WORK/fault.exe" \
    >>"$WORK/fault.build.log" 2>&1 ||
   ! "$CC" "${flags[@]}" -Dmalloc=pgy_test_string_malloc "$WORK/extent.c" \
    "$WORK/fault.c" -lm -o "$WORK/extent.exe" >"$WORK/extent.build.log" 2>&1 ||
   ! "$CC" "${flags[@]}" -Dmalloc=pgy_test_string_malloc -c "$WORK/unchecked.c" \
    -o "$WORK/unchecked.o" >"$WORK/unchecked.build.log" 2>&1 ||
   ! "$CC" "${flags[@]}" "$WORK/unchecked.o" "$WORK/fault.c" -lm -o "$WORK/unchecked.exe" \
    >>"$WORK/unchecked.build.log" 2>&1; then
    tail -n 20 "$WORK/normal.build.log" "$WORK/fault.build.log" "$WORK/extent.build.log" >&2;
    tail -n 20 "$WORK/unchecked.build.log" >&2;
    fail "emitted C did not compile";
fi
timeout 60 "$PYTHON" - "$WORK" <<'PY'
import json
from pathlib import Path
import resource
import subprocess
import sys
work = Path(sys.argv[1])
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
rows = []
for case in range(22):
    row = {'case': case}
    for mode in ('normal', 'fault', 'unchecked'):
        try:
            result = subprocess.run([str(work / f'{mode}.exe'), str(case)],
                                    capture_output=True, timeout=30)
            output = result.stdout + result.stderr
            row[mode + '_exit'] = result.returncode
        except subprocess.TimeoutExpired as failure:
            output = (failure.stdout or b'') + (failure.stderr or b'')
            row[mode + '_exit'] = 124
        (work / f'{case}.{mode}.log').write_bytes(output)
        if mode == 'normal':
            row['normal_ok'] = row['normal_exit'] == 0 and b'string-allocation-ok' in output
        elif mode == 'fault':
            row['fault_ok'] = (row['fault_exit'] == -6 and
                               b'forced-string-allocation-failure' in output and b'class=oom' in output)
        else:
            row['unchecked_ok'] = (row['unchecked_exit'] == -11 and
                                   b'forced-string-allocation-failure' in output)
    rows.append(row)
(work / 'matrix.json').write_text(json.dumps(rows, indent=2) + '\n')
bad = [row for row in rows if not all(row[key] for key in ('normal_ok', 'fault_ok', 'unchecked_ok'))]
extents = []
for case in range(4):
    result = subprocess.run([str(work / 'extent.exe'), str(case)], capture_output=True, timeout=30)
    output = result.stdout + result.stderr
    (work / f'extent.{case}.log').write_bytes(output)
    ok = (result.returncode == 0 and b'string-extent-ok' in output) if case == 0 else (
        result.returncode == -6 and b'class=oom' in output and
        b'forced-string-allocation-failure' not in output)
    extents.append({'case': case, 'exit': result.returncode, 'ok': ok})
(work / 'extents.json').write_text(json.dumps(extents, indent=2) + '\n')
if bad or any(not row['ok'] for row in extents):
    print(json.dumps(bad, indent=2), file=sys.stderr)
    print(json.dumps(extents, indent=2), file=sys.stderr)
    raise SystemExit(f'[string-allocation] FAILED; evidence={work}')
print(f'[string-allocation] PASS: 22 values/typed OOMs/unchecked faults, exact extents and 3 overflows before malloc; evidence={work}')
PY

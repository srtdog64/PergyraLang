#!/usr/bin/env bash
# Isolated sink ownership, not a claim of compiler-wide leak freedom.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON="${PYTHON_BIN:-python3}"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/json-projection-lifetime.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
fail() { echo "[json-projection-lifetime] $*; evidence=$REL" >&2; exit 1; }
checked() {
    local label="$1"; shift
    if ! timeout 60 "$@" >"$WORK/$label.log" 2>&1; then
        cat "$WORK/$label.log" >&2; fail "$label failed";
    fi
}
FIXTURE=tests/self_hosted/fixtures/json_projection_output_lifetime.pgy
checked emit-c "$PGY" --native-pipeline --emit-c "$FIXTURE" -o "$REL/probe.c"
checked emit-llvm "$PGY" --native-pipeline --emit-llvm "$FIXTURE" -o "$REL/probe.ll"
checked runtime "$CLANG" -O0 -g -fsanitize=address -fno-omit-frame-pointer \
    -DPGY_LLVM_ENABLED -Isrc -Isrc/runtime -pthread -c src/runtime/pgy_runtime_lib.c \
    -o "$WORK/runtime.o"
# Mutate generated artifacts, never production source. Borrowing a pool result
# instead of copying it must be observed after the pool has been destroyed.
"$PYTHON" - "$WORK" <<'PY'
import pathlib
import re
import sys
work = pathlib.Path(sys.argv[1])
for suffix in ('c', 'll'):
    source = (work / f'probe.{suffix}').read_text()
    if suffix == 'c':
        pattern = r'(?ms)^bool \w*JsonStringLiteralAppend\([^\n]+\)\n\{.*?^\}'
        old, new = 'pgy_array_push_owned_String(', 'pgy_array_push_String('
    else:
        pattern = r'(?ms)^define i1 @\w*JsonStringLiteralAppend\([^\n]+\) \{.*?^\}'
        old, new = '@pgy_array_push_owned_String(', '@pgy_array_push_String('
    matches = list(re.finditer(pattern, source))
    if len(matches) != 1 or matches[0].group().count(old) != 1:
        raise SystemExit(f'{suffix}: no unique typed pool-copy consumer')
    match = matches[0]
    mutated = source[:match.start()] + match.group().replace(old, new) + source[match.end():]
    (work / f'borrowed.{suffix}').write_text(mutated)
PY
for backend in c llvm; do
    suffix=c; [[ "$backend" == c ]] || suffix=ll
    for variant in probe borrowed; do
        if [[ "$backend" == c ]]; then
            checked "$backend.$variant.build" "$CC" -std=c11 -O0 -g -fsanitize=address \
                -fno-omit-frame-pointer -fwrapv -fno-strict-aliasing -Isrc -Isrc/runtime \
                -pthread "$WORK/$variant.c" -lm -o "$WORK/$backend.$variant.exe"
        else
            checked "$backend.$variant.object" "$CLANG" -O0 -g -fsanitize=address \
                -fno-omit-frame-pointer -c -x ir "$WORK/$variant.ll" -o "$WORK/$variant.o"
            # GCC supplies the installed ASan runtime; the IR and runtime TU
            # remain instrumented by Clang at -O0.
            checked "$backend.$variant.link" "$CC" -fsanitize=address \
                "$WORK/$variant.o" "$WORK/runtime.o" -pthread -lm -o "$WORK/$backend.$variant.exe"
        fi
        if [[ "$variant" == probe ]]; then
            checked "$backend.run" env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
                "$WORK/$backend.$variant.exe"
            grep -Fxq 'JSON PROJECTION LIFETIME PASS' "$WORK/$backend.run.log" ||
                fail "$backend did not complete the sink checks"
        else
            if timeout 30 env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
                "$WORK/$backend.$variant.exe" >"$WORK/$backend.borrowed.log" 2>&1; then
                fail "$backend accepted borrowed pool storage"
            fi
            grep -Fq 'AddressSanitizer: heap-use-after-free' "$WORK/$backend.borrowed.log" ||
                fail "$backend mutation failed for another reason"
        fi
    done
done
echo "[json-projection-lifetime] PASS: C/LLVM -O0 ASan+LSan; borrowed pool-copy mutation caught; evidence=$REL"

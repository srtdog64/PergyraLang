#!/usr/bin/env bash
# The returned Array owns its elements; a fresh copied push operand ends at the call.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-owned-bare-retired-return-lifetime"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
SOURCE="tests/self_hosted/fixtures/direct_mir_owned_array_string_bare_retired_return.pgy"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
for command_name in "$CC" "$CLANG" python3 timeout cmp; do
    command -v "$command_name" >/dev/null 2>&1 || fail "missing $command_name"
done
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/owned_bare_retired_return.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR"/}"
cd "$ROOT_DIR"
"$DRIVER" --emit-mir-json-verified "$SOURCE" -o "$WORK_REL/program.mir.json" \
    >"$WORK_DIR/producer.log" 2>&1 || fail "source-to-MIR refused"
python3 - "$WORK_DIR/program.mir.json" "$WORK_DIR/forged.mir.json" <<'PY'
import json
import sys

document = json.load(open(sys.argv[1], encoding="utf-8"))
build = next(row for row in document["routines"] if row["name"] == "Build")
facts = build["collection_ownership_facts"]
if len(facts) != 1 or facts[0]["origin"] != "empty-literal" or \
        facts[0]["disposition"] != "retired" or \
        facts[0]["element_ownership"] != "retired":
    raise SystemExit("source did not produce the retired return-transfer claim")
facts[0]["origin"] = "call-result"
json.dump(document, open(sys.argv[2], "w", encoding="utf-8"))
PY
printf '2\nab\nfixed\n' >"$WORK_DIR/expected"
"$CLANG" -DPGY_LLVM_ENABLED -Isrc -Isrc/runtime -O0 -g \
    -fsanitize=address -fno-omit-frame-pointer -c src/runtime/pgy_runtime_lib.c \
    -o "$WORK_DIR/runtime.o" >"$WORK_DIR/runtime.compile.log" 2>&1 ||
    fail "LLVM runtime did not compile"
for backend in c llvm; do
    extension=c
    [[ "$backend" != llvm ]] || extension=ll
    artifact="$WORK_DIR/program.$extension"
    "$DRIVER" "--mir-json-backend=$backend" "$WORK_REL/program.mir.json" \
        -o "$WORK_REL/program.$extension" >"$WORK_DIR/$backend.project.log" 2>&1 ||
        fail "$backend refused retired bare return"
    [[ -s "$artifact" ]] || fail "$backend emitted no artifact"
    if [[ "$backend" == c ]]; then
        "$CC" -x c -std=c11 -O0 -g -fsanitize=address -fno-omit-frame-pointer \
            -Isrc -Isrc/runtime -pthread "$artifact" -lm -o "$WORK_DIR/c.exe" \
            >"$WORK_DIR/c.compile.log" 2>&1 || fail "C artifact did not compile"
    else
        "$CLANG" -x ir -O0 -g -fsanitize=address -fno-omit-frame-pointer \
            "$artifact" -x none "$WORK_DIR/runtime.o" -pthread -lm \
            -o "$WORK_DIR/llvm.exe" >"$WORK_DIR/llvm.compile.log" 2>&1 ||
            fail "LLVM artifact did not compile"
    fi
    ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 timeout 30 "$WORK_DIR/$backend.exe" \
        >"$WORK_DIR/$backend.run" 2>"$WORK_DIR/$backend.sanitizer.log" ||
        fail "$backend leaked or failed under ASan/LSan"
    cmp -s "$WORK_DIR/expected" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
    if "$DRIVER" "--mir-json-backend=$backend" "$WORK_REL/forged.mir.json" \
        -o "$WORK_REL/forged.$extension" >"$WORK_DIR/$backend.forged.log" 2>&1; then
        fail "$backend accepted a forged retired origin"
    fi
    [[ ! -e "$WORK_DIR/forged.$extension" ]] ||
        fail "$backend published a forged artifact"
    grep -Fq 'CODEGEN ERROR:' "$WORK_DIR/$backend.forged.log" ||
        fail "$backend lost the forged-origin refusal diagnostic"
done
python3 - "$WORK_DIR/program.c" "$WORK_DIR/omitted-retire.c" <<'PY'
from pathlib import Path
import sys

text = Path(sys.argv[1]).read_text()
release = 'free((void *)pgy_owned_push_value);'
if text.count(release) != 1:
    raise SystemExit("expected one fresh push source to retire")
Path(sys.argv[2]).write_text(text.replace(release, '', 1))
PY
"$CC" -x c -std=c11 -O0 -g -fsanitize=address -fno-omit-frame-pointer \
    -Isrc -Isrc/runtime -pthread "$WORK_DIR/omitted-retire.c" -lm \
    -o "$WORK_DIR/omitted-retire.exe" >"$WORK_DIR/mutant.compile.log" 2>&1 ||
    fail "omitted-retire mutant did not compile"
if ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 timeout 30 \
        "$WORK_DIR/omitted-retire.exe" >"$WORK_DIR/mutant.run" \
        2>"$WORK_DIR/mutant.sanitizer.log"; then
    fail "omitted-retire mutant escaped LSan"
fi
grep -Fq 'LeakSanitizer: detected memory leaks' "$WORK_DIR/mutant.sanitizer.log" ||
    fail "mutant failed for a reason other than its leaked source"
echo "[$LABEL] C/LLVM returned cleanup, fresh source retirement and refusal: PASS"

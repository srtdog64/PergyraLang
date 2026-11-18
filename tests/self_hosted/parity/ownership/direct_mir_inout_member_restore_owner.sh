#!/usr/bin/env bash
# A moved collection field returns to the exact inout record that supplied it.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
SOURCE="tests/self_hosted/fixtures/direct_mir_inout_member_restore.pgy"
THREE_DROP="tests/self_hosted/fixtures/direct_mir_inout_member_three_drop_restore.pgy"
HANDED="tests/self_hosted/fixtures/direct_mir_inout_member_restore_handoff_refused.pgy"
fail() { echo "[self-host-inout-member-restore] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "self-host-inout-member-restore" "$DRIVER" || exit 1
for command_name in "$CC" "$CLANG" python3 timeout cmp; do
    command -v "$command_name" >/dev/null 2>&1 || fail "missing $command_name"
done
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/inout_member_restore.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR"/}"
cd "$ROOT_DIR"
"$DRIVER" --emit-mir-json-verified "$SOURCE" -o "$WORK_REL/program.mir.json" \
    >"$WORK_DIR/producer.log" 2>&1 || fail "source-to-MIR refused"
"$DRIVER" --emit-mir-json-verified "$THREE_DROP" \
    -o "$WORK_REL/three_drop.mir.json" >"$WORK_DIR/three_drop.producer.log" 2>&1 ||
    fail "three-drop source-to-MIR refused"
if "$DRIVER" --emit-mir-json-verified "$HANDED" \
        -o "$WORK_REL/handed.mir.json" >"$WORK_DIR/handed.log" 2>&1; then
    fail "callee hand-off was restored into the record"
fi
[[ ! -e "$WORK_DIR/handed.mir.json" ]] || fail "hand-off published MIR"
grep -Fq 'owned_argument_storage_not_live' "$WORK_DIR/handed.log" ||
    fail "hand-off lost its ownership diagnostic"
python3 - "$WORK_DIR/program.mir.json" "$WORK_DIR" <<'PY'
import json
import pathlib
import sys

document = json.load(open(sys.argv[1], encoding="utf-8"))
grow = next(row for row in document["routines"] if row["name"] == "Grow")
facts = grow["collection_ownership_facts"]
param = grow["params"][0]
if len(facts) != 1 or facts[0]["origin"] != "member-move" or \
        facts[0]["disposition"] != "retired" or \
        param["carriage"] != "value-result" or \
        facts[0]["source_binding_syntax_id"] != param["source_syntax_id"]:
    raise SystemExit("producer did not issue the inout member restoration claim")
work = pathlib.Path(sys.argv[2])
source_id = facts[0]["source_binding_syntax_id"]
facts[0]["source_binding_syntax_id"] = source_id + 1000000
(work / "forged_source.mir.json").write_text(json.dumps(document), encoding="utf-8")
facts[0]["source_binding_syntax_id"] = source_id
facts[0]["origin_syntax_id"] += 1000000
(work / "forged_field.mir.json").write_text(json.dumps(document), encoding="utf-8")
PY
printf '2\n' >"$WORK_DIR/program.expected"
printf '0\n' >"$WORK_DIR/three_drop.expected"
"$CLANG" -DPGY_LLVM_ENABLED -Isrc -Isrc/runtime -O0 -g \
    -fsanitize=address -fno-omit-frame-pointer -c src/runtime/pgy_runtime_lib.c \
    -o "$WORK_DIR/runtime.o" >"$WORK_DIR/runtime.compile.log" 2>&1 ||
    fail "LLVM runtime did not compile"
for backend in c llvm; do
    extension=c
    [[ "$backend" != llvm ]] || extension=ll
    for case_name in program three_drop; do
        "$DRIVER" "--mir-json-backend=$backend" "$WORK_REL/$case_name.mir.json" \
            -o "$WORK_REL/$case_name.$extension" \
            >"$WORK_DIR/$backend.$case_name.project.log" 2>&1 ||
            fail "$backend refused $case_name restoration"
        [[ -s "$WORK_DIR/$case_name.$extension" ]] ||
            fail "$backend emitted no $case_name artifact"
        if [[ "$backend" == c ]]; then
            "$CC" -x c -std=c11 -O0 -g -fsanitize=address -fno-omit-frame-pointer \
                -Isrc -Isrc/runtime -pthread "$WORK_DIR/$case_name.c" -lm \
                -o "$WORK_DIR/$backend.$case_name.exe" \
                >"$WORK_DIR/$backend.$case_name.compile.log" 2>&1 ||
                fail "C $case_name artifact did not compile"
        else
            "$CLANG" -x ir -O0 -g -fsanitize=address -fno-omit-frame-pointer \
                "$WORK_DIR/$case_name.ll" -x none "$WORK_DIR/runtime.o" -pthread -lm \
                -o "$WORK_DIR/$backend.$case_name.exe" \
                >"$WORK_DIR/$backend.$case_name.compile.log" 2>&1 ||
                fail "LLVM $case_name artifact did not compile"
        fi
        leak_check=1
        # The nonempty record still lacks automatic field retirement (S2).
        [[ "$case_name" != program ]] || leak_check=0
        ASAN_OPTIONS=detect_leaks=$leak_check:halt_on_error=1 timeout 30 \
            "$WORK_DIR/$backend.$case_name.exe" >"$WORK_DIR/$backend.$case_name.run" \
            2>"$WORK_DIR/$backend.$case_name.sanitizer.log" ||
            fail "$backend $case_name failed ASan/LSan"
        cmp -s "$WORK_DIR/$case_name.expected" "$WORK_DIR/$backend.$case_name.run" ||
            fail "$backend $case_name runtime output drifted"
    done
    # The MIR integrity boundary rejects these forged facts before transition
    # admission. The positive case above exercises the new transition path.
    for forged in forged_source forged_field; do
        if "$DRIVER" "--mir-json-backend=$backend" "$WORK_REL/$forged.mir.json" \
                -o "$WORK_REL/$forged.$extension" >"$WORK_DIR/$backend.$forged.log" 2>&1; then
            fail "$backend accepted $forged"
        fi
        [[ ! -e "$WORK_DIR/$forged.$extension" ]] ||
            fail "$backend published $forged"
        grep -Fq 'CODEGEN ERROR:' "$WORK_DIR/$backend.$forged.log" ||
            fail "$backend lost $forged diagnostic"
    done
done
echo "[self-host-inout-member-restore] C/LLVM restoration and forged MIR refusal: PASS"

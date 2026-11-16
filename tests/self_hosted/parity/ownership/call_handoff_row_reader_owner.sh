#!/usr/bin/env bash
# Both front ends publish each routine's call hand-off rows in MIR JSON
# (slice S2, docs/audits/unified_handoff_rule_study_2026-11-16.md section 3),
# and the installed driver's MIR reader admits a document only when every
# routine's rows are well formed in the vocabulary of
# src/semantic/call_handoff_rule.def. This gate takes the native and the
# self-host MIR of one program with a constructor hand-off and a callee
# formal, checks that the installed driver admits both, then forges rows the
# rule cannot produce and checks the driver refuses each one at admission,
# naming the refusing stage, with no artifact: an unknown action or member, a
# class set that cannot produce the stated action, a duplicate action or
# class row, a divergence that changes nothing or names the wrong form, a
# compiler Clone that is not a fresh value, a handed-off row that is not a
# hand-off, a wrong count and a missing object.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-call-handoff-row-reader"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
REFUSAL="MIR call hand-off rows are missing or invalid: "

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/call-handoff-row-reader.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR/"}"

# Pack takes `texts` once (a hand-off) and Count reads its formal (a class
# row on Count, a lending action row in Main).
cat >"$WORK_DIR/handoff.pgy" <<'EOF'
struct Pack { texts: Array<String>; }
func Count(values: Array<String>) -> Int {
    return ArrayLength(values);
}
func Main() -> Void {
    var texts: Array<String> = [];
    ArrayPushOwnedString(texts, Concat("owned", "-text"));
    var names: Array<String> = [];
    ArrayPushOwnedString(names, Concat("a", "b"));
    Log(Count(names));
    let pack: Pack = Pack(texts);
    Log(ArrayLength(pack.texts));
    Log(ArrayLength(names));
}
EOF

(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle "$WORK_REL/handoff.pgy") \
    >"$WORK_DIR/native.json" 2>"$WORK_DIR/native.err" ||
    fail "native MIR producer rejected the program: $(tail -3 "$WORK_DIR/native.err")"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$WORK_REL/handoff.pgy" \
    -o "$WORK_REL/self.json") >"$WORK_DIR/self.out" 2>"$WORK_DIR/self.err" || {
    cat "$WORK_DIR/self.out" "$WORK_DIR/self.err" >&2
    fail "installed self-host MIR producer rejected the program"
}

# Each producer's own rows: the constructor hand-off, and the formal it read.
python3 - "$WORK_DIR/native.json" "$WORK_DIR/self.json" <<'PY' ||
import json
import sys

for path in sys.argv[1:]:
    document = json.load(open(path, encoding="utf-8"))
    routines = {row["name"]: row for row in document["routines"]}
    main = routines["Main"]["call_handoff"]
    handed = [row for row in main["actions"]
              if row["callee"] == "Pack" and row["action"] == "HAND_OFF"
              and row["row"] == "handed-off" and row["members"] == ["KEEP"]]
    if len(handed) != 1 or not main["complete"]:
        raise SystemExit(f"{path}: no single constructor hand-off row: {main}")
    classes = routines["Count"]["call_handoff"]["classes"]
    if len(classes) != 1 or classes[0]["parameter_index"] != 0:
        raise SystemExit(f"{path}: Count has no class row for its formal: {classes}")
PY
    fail "a producer lost its call hand-off rows"

# Both producers' MIR passes the reader's admission.
for producer in native self; do
    (cd "$ROOT_DIR" && "$DRIVER" --mir-json "$WORK_REL/$producer.json" \
        -o "$WORK_REL/$producer.c") >"$WORK_DIR/$producer.read" 2>&1 || true
    if grep -Fq "$REFUSAL" "$WORK_DIR/$producer.read"; then
        cat "$WORK_DIR/$producer.read" >&2
        fail "the reader refused the $producer producer's rows"
    fi
done

mkdir -p "$WORK_DIR/forged"
python3 - "$WORK_DIR/native.json" "$WORK_DIR/self.json" "$WORK_DIR/forged" <<'PY'
import copy
import json
import pathlib
import sys

target = pathlib.Path(sys.argv[3])

def handoff_row(main):
    return next(row for row in main["call_handoff"]["actions"]
                if row["callee"] == "Pack" and row["row"] == "handed-off")

def emit(producer, name, stage, change):
    document = json.loads(pathlib.Path(sys.argv[1 if producer == "native" else 2]).read_text(encoding="utf-8"))
    main = next(row for row in document["routines"] if row["name"] == "Main")
    change(main)
    (target / f"{producer}.{name}.{stage}.json").write_text(
        json.dumps(document, separators=(",", ":")) + "\n", encoding="utf-8")

def set_field(field, value):
    return lambda main: handoff_row(main).__setitem__(field, value)

def duplicate_action(main):
    rows = main["call_handoff"]["actions"]
    rows.append(copy.deepcopy(handoff_row(main)))
    rows[-1]["row"] = "kept"
    main["call_handoff"]["action_count"] = len(rows)

def duplicate_class(main):
    row = {"parameter_index": 0, "members": ["READ"], "divergence": ""}
    main["call_handoff"]["classes"] = [row, dict(row)]
    main["call_handoff"]["class_count"] = 2

def handed_off_copy(main):
    row = handoff_row(main)
    row.update({"later": "USED_AGAIN", "action": "CLONE_ARGUMENT"})

def divergence_without_effect(main):
    row = handoff_row(main)
    row.update({"later": "USED_AGAIN", "action": "CLONE_ARGUMENT", "row": "kept",
                "divergence": "SELF_RETIRED_CONSTRUCTOR_INPUT_KEPT"})

def class_row_divergence(main):
    main["call_handoff"]["classes"] = [{"parameter_index": 0, "members": ["READ"],
                                        "divergence": "NATIVE_KEEP_REUSE_REFUSED"}]
    main["call_handoff"]["class_count"] = 1

def clone_not_fresh(main):
    row = handoff_row(main)
    row.update({"compiler_clone": True, "row": "kept"})

def count_mismatch(main):
    main["call_handoff"]["action_count"] += 1

for producer in ("native", "self"):
    emit(producer, "unknown_action", "call_handoff_action_vocabulary",
         set_field("action", "TELEPORT"))
    emit(producer, "unknown_member", "call_handoff_action_vocabulary",
         set_field("members", ["KEEP", "TELEPORT"]))
    emit(producer, "class_cannot_produce_action", "call_handoff_action_rule",
         set_field("members", ["READ"]))
    emit(producer, "duplicate_action", "call_handoff_action_duplicate", duplicate_action)
    emit(producer, "duplicate_class", "call_handoff_class_duplicate", duplicate_class)
    emit(producer, "handed_off_not_hand_off", "call_handoff_action_site", handed_off_copy)
    emit(producer, "divergence_without_effect", "call_handoff_action_rule",
         divergence_without_effect)
    emit(producer, "class_row_divergence_form", "call_handoff_class_vocabulary",
         class_row_divergence)
    emit(producer, "clone_not_fresh", "call_handoff_action_site", clone_not_fresh)
    emit(producer, "count_mismatch", "call_handoff_action_count", count_mismatch)
    emit(producer, "missing_object", "call_handoff_missing",
         lambda main: main.pop("call_handoff"))
PY

refused=0
for forged in "$WORK_DIR"/forged/*.json; do
    name="$(basename "$forged" .json)"
    stage="${name##*.}"
    # The source-to-C reader route and the direct MIR LLVM route.
    for route in c llvm; do
        flag=--mir-json
        [[ "$route" == llvm ]] && flag=--mir-json-backend=llvm
        artifact="$WORK_REL/forged/$name.$route"
        set +e
        (cd "$ROOT_DIR" && "$DRIVER" "$flag" "${forged#"$ROOT_DIR/"}" -o "$artifact") \
            >"$WORK_DIR/forged/$name.$route.log" 2>&1
        rc=$?
        set -e
        [[ "$rc" -ne 0 ]] || fail "$name was admitted by $flag"
        [[ ! -e "$ROOT_DIR/$artifact" ]] || fail "$name published an artifact on $flag"
        grep -Fq "$REFUSAL$stage" "$WORK_DIR/forged/$name.$route.log" || {
            head -5 "$WORK_DIR/forged/$name.$route.log" >&2
            fail "$name was refused for another reason on $flag"
        }
        refused=$((refused + 1))
    done
done
[[ "$refused" -eq 44 ]] || fail "forged corpus is incomplete: $refused refusals"

echo "[$LABEL] PASS native and self-host rows admitted; $refused forged-row refusals"

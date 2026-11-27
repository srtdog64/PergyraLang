#!/usr/bin/env bash
# Compiler Clone landings carried through self-host MIR to the direct routes
# (mir/clone_site_projection_owner.pgy, direct MIR
# direct_mir_scalar_program_compiler_clone_site_owner.pgy). The self-host MIR
# lists each landing as `expr0_clone_sites` / `expr1_clone_sites`; direct C and
# LLVM copy it as an explicit Clone would, so an Array, String or record a
# later write would otherwise share runs as native does, with no memory error
# under AddressSanitizer. A record is copied field by field
# (direct_mir_scalar_program_logical_record_clone_owner.pgy), as an explicit
# Clone of a record is. The installed default LLVM route takes the same copies.
# A site of a type direct MIR cannot copy (a record with a map field), a record
# Clone of a value that is not a place, a malformed list, an index past the
# graph, and a site in a lane no admission reads are refused on both backends.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-compiler-clone-carriage"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/compiler_clone_carriage"
WORK_DIR="$ROOT_DIR/$WORK_REL"
FIXTURES="tests/cases/function_param_flow_summary"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
[[ -x "$PGY" ]] || fail "missing compiler: $PGY"
[[ -x "$DRIVER" ]] || fail "missing installed self-host driver: $DRIVER"
rm -rf "$WORK_DIR"; mkdir -p "$WORK_DIR"

# Each of the first programs has one landing: an Array binding initializer, an
# Array argument beside the same Array given inout (before and after it), a
# String element kept by the callee, and a String element given to an opaque
# call. The record programs land a record argument a callee grows (twice, on
# each loop pass, and before a read of the caller's copy) or whose field alias
# an inout callee writes.
STEMS="local_copy_source_sorted_landed_valid by_value_argument_before_its_inout_landed_valid by_value_argument_beside_its_inout_landed_valid string_element_kept_by_callee_landed_valid string_element_tostring_opaque_call_landed_valid"
RECORD_STEMS="default_aggregate_move_reuse_landed_valid default_aggregate_move_loop_reuse_landed_valid by_value_formal_array_sorted_landed_valid by_value_field_alias_written_by_inout_callee_landed_valid"
for stem in $STEMS $RECORD_STEMS; do cp "$ROOT_DIR/$FIXTURES/$stem.pgy" "$WORK_DIR/$stem.pgy"; done
# A nested record (a record, an Int, an Array<Int>, a String and an
# Array<String> inside) landed at a growing callee's argument.
cat >"$WORK_DIR/nested_record_landed.pgy" <<'EOF'
struct Inner {
    n: Int;
    ids: Array<Int>;
}

struct Outer {
    inner: Inner;
    tag: String;
    names: Array<String>;
}

func Grow(o: Outer) -> Outer {
    var ids: Array<Int> = o.inner.ids;
    var names: Array<String> = o.names;
    ArraySet(ids, 0, 9);
    ArrayPush(ids, 5);
    ArrayPush(names, "z");
    return Outer(Inner(o.inner.n + 1, ids), "new", names);
}

func Main() -> Int {
    let o: Outer = Outer(Inner(2, [1, 2]), "t", ["a"]);
    let p: Outer = Grow(o);
    Print(ToString(ArrayLength(o.inner.ids) + ArrayLength(p.inner.ids) + ArrayLength(o.names) + ArrayLength(p.names)));
    Print(ToString(o.inner.ids[0] * 10 + p.inner.n));
    Print(o.tag);
    return 0;
}
EOF
# An explicit Clone of a three-field record: writing the copy's Array leaves
# the source's as it was.
cat >"$WORK_DIR/explicit_record_clone.pgy" <<'EOF'
struct Inner {
    n: Int;
    tag: String;
}

struct Outer {
    inner: Inner;
    ids: Array<Int>;
    flag: Bool;
}

func Main() -> Int {
    let p: Outer = Outer(Inner(4, "t"), [1, 2], true);
    let q: Outer = Clone(p);
    var ids: Array<Int> = q.ids;
    ArraySet(ids, 0, 9);
    Print(ToString(p.ids[0] * 10 + ids[0]));
    Print(q.inner.tag);
    Print(ToString(q.inner.n));
    return 0;
}
EOF

cat >"$WORK_DIR/owned_field_copy_set.pgy" <<'EOF'
struct Calls { target_names: Array<String>; }
struct Graph { arena: Calls; }

func MakeGraph() -> Graph { return Graph(Calls(["original"])); }

func Observe(ref graph: Graph) -> String { return graph.arena.target_names[0]; }

func Main() -> Void {
    let graph = MakeGraph();
    var target_names: Array<String> = graph.arena.target_names;
    ArraySet(target_names, 0, "changed");
    Log(Observe(graph));
    Log(target_names[0]);
}
EOF
"$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$WORK_DIR/runtime.o" 2>/dev/null ||
    fail "runtime object did not compile"
for stem in $STEMS $RECORD_STEMS nested_record_landed explicit_record_clone owned_field_copy_set; do
    "$PGY" --native-pipeline --backend=c "$WORK_DIR/$stem.pgy" -o "$WORK_DIR/$stem.native.exe" \
        >/dev/null 2>"$WORK_DIR/$stem.native.err" || fail "$stem native compile failed"
    "$WORK_DIR/$stem.native.exe" | tr -d '\r' >"$WORK_DIR/$stem.native.run"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$WORK_REL/$stem.pgy" \
        -o "$WORK_REL/$stem.mir.json") >/dev/null 2>"$WORK_DIR/$stem.mir.err" ||
        fail "$stem MIR production failed"
    [[ "$stem" == explicit_record_clone ]] ||
        grep -Eq '"expr[01]_clone_sites":\[[0-9]' "$WORK_DIR/$stem.mir.json" ||
        fail "$stem MIR carries no compiler Clone site"
    for backend in c llvm; do
        suffix=c; [[ "$backend" == llvm ]] && suffix=ll
        (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$WORK_REL/$stem.mir.json" \
            -o "$WORK_REL/$stem.direct.$suffix") >"$WORK_DIR/$stem.direct-$backend.out" 2>&1 ||
            fail "$stem direct $backend failed: $(grep -m1 -o 'CODEGEN ERROR.*' "$WORK_DIR/$stem.direct-$backend.out")"
    done
    # Memory errors only: the copies leak as an explicit Clone's do on these
    # routes (the direct-MIR Array and String local release is open).
    "$CC" -x c -std=c11 -g -fsanitize=address -fno-omit-frame-pointer \
        -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread "$WORK_DIR/$stem.direct.c" -lm \
        -o "$WORK_DIR/$stem.direct-c.asan" >/dev/null 2>"$WORK_DIR/$stem.direct-c.compile.err" ||
        fail "$stem direct C did not compile"
    ASAN_OPTIONS=detect_leaks=0 "$WORK_DIR/$stem.direct-c.asan" 2>"$WORK_DIR/$stem.direct-c.asan.err" |
        tr -d '\r' >"$WORK_DIR/$stem.direct-c.run" || fail "$stem direct C failed under AddressSanitizer"
    cmp -s "$WORK_DIR/$stem.direct-c.run" "$WORK_DIR/$stem.native.run" || fail "$stem direct C runs unlike native"
    "$CLANG" -fsanitize=address -x ir "$WORK_DIR/$stem.direct.ll" -x none "$WORK_DIR/runtime.o" -pthread -lm \
        -o "$WORK_DIR/$stem.direct-llvm.asan" >/dev/null 2>"$WORK_DIR/$stem.direct-llvm.compile.err" ||
        fail "$stem direct LLVM did not link"
    ASAN_OPTIONS=detect_leaks=0 "$WORK_DIR/$stem.direct-llvm.asan" 2>"$WORK_DIR/$stem.direct-llvm.asan.err" |
        tr -d '\r' >"$WORK_DIR/$stem.direct-llvm.run" || fail "$stem direct LLVM failed under AddressSanitizer"
    cmp -s "$WORK_DIR/$stem.direct-llvm.run" "$WORK_DIR/$stem.native.run" || fail "$stem direct LLVM runs unlike native"
done

"$PYTHON_BIN" - "$WORK_DIR/owned_field_copy_set.mir.json" "$WORK_DIR" <<'PY'
import copy
import json
from pathlib import Path
import sys

source, output = map(Path, sys.argv[1:])
program = json.loads(source.read_text(encoding="utf-8"))
fixture = (output / "owned_field_copy_set.pgy").read_text(encoding="utf-8")
loaned = fixture.replace('    ArraySet(target_names', '    let view = target_names[0];\n    ArraySet(target_names')
loaned = loaned.replace('    Log(target_names[0]);', '    Log(view);')
(output / "owned_field_copy_loan.pgy").write_text(loaned, encoding="utf-8")
for mutation in ("missing_initializer_copy", "missing_owned_set_receipt", "wrong_set_receiver"):
    broken = copy.deepcopy(program)
    main = next(routine for routine in broken["routines"] if routine["name"] == "Main")
    instructions = [row for block in main["blocks"] for row in block["instructions"]]
    if mutation == "missing_initializer_copy":
        definition = next(row for row in instructions if row.get("arg0") == "target_names")
        del definition["expr0_clone_sites"]
    else:
        store = next(row for row in instructions if row.get("arg0") == "ArraySet")
        receipt = store["collection_ownership_receipt"]
        assert receipt["kind"] == "owned-string-set"
        if mutation == "missing_owned_set_receipt":
            store["collection_ownership_receipt"] = None
        else:
            graph = next(row for row in main["source_locals"] if row["name"] == "graph")
            receipt["receiver_binding_syntax_id"] = graph["binding_syntax_id"]
    (output / f"{mutation}.mir.json").write_text(
        json.dumps(broken, separators=(",", ":")), encoding="utf-8")
PY

for mutation in missing_initializer_copy missing_owned_set_receipt wrong_set_receiver; do
    for backend in c llvm; do
        output="$WORK_REL/$mutation.$backend.out"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
                "$WORK_REL/$mutation.mir.json" -o "$output") \
                >"$WORK_DIR/$mutation.$backend.log" 2>&1; then
            fail "direct $backend accepted an owned field copy with $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output" ]] || fail "direct $backend emitted an artifact with $mutation"
    done
done

if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$WORK_REL/owned_field_copy_loan.pgy" \
        -o "$WORK_REL/owned_field_copy_loan.mir.json") >"$WORK_DIR/owned_field_copy_loan.log" 2>&1; then
    fail "a loan of the element the owned set releases escaped the source check"
fi
[[ ! -e "$WORK_DIR/owned_field_copy_loan.mir.json" ]] || fail "the rejected loan published MIR"
grep -Fq 'indexed_string_use_after_element_release' "$WORK_DIR/owned_field_copy_loan.log" ||
    fail "the loan was refused before its element release check"

# The installed default LLVM route copies the Array before it is sorted.
stem=local_copy_source_sorted_landed_valid
PGY_SELF_DRIVER_BIN="$DRIVER" "$PGY" --backend=llvm "$WORK_DIR/$stem.pgy" -o "$WORK_DIR/$stem.default.exe" \
    >"$WORK_DIR/$stem.default.out" 2>&1 || fail "$stem default LLVM route failed"
"$WORK_DIR/$stem.default.exe" | tr -d '\r' >"$WORK_DIR/$stem.default.run"
cmp -s "$WORK_DIR/$stem.default.run" "$WORK_DIR/$stem.native.run" || fail "$stem default LLVM route runs unlike native"

# Forged sites on the binding program: a malformed list, an index past the
# graph, an Int node (no Clone kind), and a lane no admission reads.
"$PYTHON_BIN" - "$WORK_DIR/$stem.mir.json" "$WORK_DIR" <<'PY'
import json, sys
src, out = sys.argv[1:]
doc = json.load(open(src, encoding="utf-8"))
def instructions(d):
    for r in d["routines"]:
        for b in r["blocks"]:
            for i in b["instructions"]:
                yield i
for how in ("malformed", "window", "int", "lane"):
    d = json.loads(json.dumps(doc))
    site = next(i for i in instructions(d) if i.get("expr0_clone_sites"))
    if how == "malformed":
        site["expr0_clone_sites"] = [0, 0]
    elif how == "window":
        site["expr0_clone_sites"] = [len(site["expr0_graph"]["nodes"]) + 3]
    elif how == "int":
        printing = next(i for i in instructions(d) if str(i.get("expr0", "")).startswith("Print("))
        nodes = printing["expr0_graph"]["nodes"]
        printing["expr0_clone_sites"] = [next(k for k, n in enumerate(nodes)
                                              if n["kind"] == "integer_literal" and n["text"] == "10")]
    else:
        if site.get("expr1_graph"):
            raise SystemExit("the binding instruction has an expr1 graph")
        site["expr1_clone_sites"] = [0]
    open("%s/forged_%s.mir.json" % (out, how), "w", encoding="utf-8").write(json.dumps(d, separators=(",", ":")))
PY
# A record with a map field has no direct-MIR copy; an explicit Clone of a
# record call result is not a place the field-by-field copy may read again.
for refused in local_copy_map_held_at_field_changed_landed_valid:clone-site-type \
    default_aggregate_move_cloned_growth_landed_valid:record-clone-source; do
    stem="${refused%%:*}"; expect="stage=${refused##*:}"
    cp "$ROOT_DIR/$FIXTURES/$stem.pgy" "$WORK_DIR/$stem.pgy"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$WORK_REL/$stem.pgy" \
        -o "$WORK_REL/$stem.mir.json") >/dev/null 2>"$WORK_DIR/$stem.mir.err" ||
        fail "$stem MIR production failed"
    for backend in c llvm; do
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$WORK_REL/$stem.mir.json" \
            -o "$WORK_REL/$stem.direct.$backend.out") >"$WORK_DIR/$stem.direct-$backend.log" 2>&1; then
            fail "direct $backend copied $stem's record without a copy for each field"
        fi
        grep -Fq "$expect" "$WORK_DIR/$stem.direct-$backend.log" ||
            fail "direct $backend refused $stem for another reason: $(grep -m1 -o 'CODEGEN ERROR.*' "$WORK_DIR/$stem.direct-$backend.log")"
    done
done

for how in malformed window int lane; do
    case "$how" in
        malformed) expect='clone-site-list' ;;
        window) expect='clone-site-window' ;;
        int) expect='clone-site-type' ;;
        lane) expect='compiler Clone site was copied by no admission' ;;
    esac
    for backend in c llvm; do
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$WORK_REL/forged_$how.mir.json" \
            -o "$WORK_REL/forged_$how.$backend.out") >"$WORK_DIR/forged_$how.$backend.log" 2>&1; then
            fail "direct $backend accepted a forged compiler Clone site ($how)"
        fi
        grep -Fq "$expect" "$WORK_DIR/forged_$how.$backend.log" ||
            fail "direct $backend refused the forged compiler Clone site ($how) for another reason: $(grep -m1 -o 'CODEGEN ERROR.*' "$WORK_DIR/forged_$how.$backend.log")"
    done
done
echo "[$LABEL] compiler and explicit copies run as native on C and LLVM, including the owned field copy and subsequent set. Unsupported copies, four forged sites and missing field copy or owned-set evidence refused on both backends."

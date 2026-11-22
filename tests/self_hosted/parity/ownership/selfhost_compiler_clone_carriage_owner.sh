#!/usr/bin/env bash
# Compiler Clone landings carried through self-host MIR to the direct routes
# (mir/clone_site_projection_owner.pgy, direct MIR
# direct_mir_scalar_program_compiler_clone_site_owner.pgy). The self-host MIR
# lists each landing as `expr0_clone_sites` / `expr1_clone_sites`; direct C and
# LLVM copy it as an explicit Clone would, so an Array or String a later write
# would otherwise share runs as native does, with no memory error under
# AddressSanitizer. The installed default LLVM route takes the same copies.
# A site of a type direct MIR cannot copy, a malformed list, an index past the
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

# Each program has one landing: an Array binding initializer, an Array
# argument beside the same Array given inout (before and after it), a String
# element kept by the callee, and a String element given to an opaque call.
STEMS="local_copy_source_sorted_landed_valid by_value_argument_before_its_inout_landed_valid by_value_argument_beside_its_inout_landed_valid string_element_kept_by_callee_landed_valid string_element_tostring_opaque_call_landed_valid"
"$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$WORK_DIR/runtime.o" 2>/dev/null ||
    fail "runtime object did not compile"
for stem in $STEMS; do
    cp "$ROOT_DIR/$FIXTURES/$stem.pgy" "$WORK_DIR/$stem.pgy"
    "$PGY" --native-pipeline --backend=c "$WORK_DIR/$stem.pgy" -o "$WORK_DIR/$stem.native.exe" \
        >/dev/null 2>"$WORK_DIR/$stem.native.err" || fail "$stem native compile failed"
    "$WORK_DIR/$stem.native.exe" | tr -d '\r' >"$WORK_DIR/$stem.native.run"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$WORK_REL/$stem.pgy" \
        -o "$WORK_REL/$stem.mir.json") >/dev/null 2>"$WORK_DIR/$stem.mir.err" ||
        fail "$stem MIR production failed"
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
echo "[$LABEL] 5 compiler Clone landings (Array binding and arguments, String elements) copied on direct C and LLVM from the MIR sites and on the default LLVM route, run as native with no memory error; 4 forged sites refused on both backends"

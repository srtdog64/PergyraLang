#!/usr/bin/env bash
# Self-host region allocation on the installed source C route
# (semantic/region_escape_site_owner.pgy, codegen region_scope_emit_owner.pgy).
# A String `+` spine given straight to a call-duration consumer comes from the
# function region, which every return frees: the programs run as native does
# and LeakSanitizer reports nothing. A concatenation that outlives the call
# (bound, returned, stored) stays on the heap: its C names no region.
# The direct-MIR C and LLVM routes read the sites the MIR carries
# (`expr0_region_sites`) and do the same, freeing the region on the `?` early
# return too (LeakSanitizer on C and LLVM); a site the MIR forges onto a node
# that is not such a concatenation, a malformed list and an index past the
# graph are refused.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-region-allocation"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/region_allocation"
WORK_DIR="$ROOT_DIR/$WORK_REL"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
[[ -x "$PGY" ]] || fail "missing compiler: $PGY"
[[ -x "$DRIVER" ]] || fail "missing installed self-host driver: $DRIVER"
rm -rf "$WORK_DIR"; mkdir -p "$WORK_DIR"

cat >"$WORK_DIR/region_calls.pgy" <<'PGY'
struct Box {
    label: String;
}

func Sink(ref value: String) -> Void {
    Log(value);
}

func Describe(ref box: Box, count: Int) -> Int {
    Log("box " + box.label + " count");
    Print("plain " + box.label);
    let n: Int = StringLength("len " + box.label);
    Sink("sink " + box.label);
    if count > 1 {
        Log("early " + box.label);
        return n + count;
    }
    return n;
}

func Main() -> Void {
    let box: Box = Box("crate");
    var total: Int = 0;
    var i: Int = 0;
    while i < 3 {
        total = total + Describe(box, i);
        i = i + 1;
    }
    Log("done " + "here");
}
PGY
cat >"$WORK_DIR/heap_escape.pgy" <<'PGY'
func Label(prefix: String) -> String {
    return prefix + "-tail";
}

func Main() -> Void {
    let bound: String = "bound " + "value";
    var names: Array<String> = [];
    ArrayPush(names, "stored " + "value");
    Log(bound);
    Log(Label("head"));
    Log(names[0]);
}
PGY
# region_try: the `?` early return is an exit too.
cat >"$WORK_DIR/region_try.pgy" <<'PGY'
func PickTryValue(x: Int) -> Option<Int> {
    if x == 0 {
        return None;
    }
    return Some(x);
}

func DoubleTryValue(x: Int) -> Option<Int> {
    Log("try " + ToString(x));
    let found: Int = PickTryValue(x)?;
    return Some(found + found);
}

func Main() -> Void {
    let present: Option<Int> = DoubleTryValue(4);
    if IsSome(present) {
        Log(UnwrapOption(present));
    }
    let absent: Option<Int> = DoubleTryValue(0);
    if !IsSome(absent) {
        Log(0);
    }
}
PGY
STEMS="region_calls heap_escape region_try"

# region_calls: the region carries its sites, every exit frees it, no leak.
for stem in $STEMS; do
    (cd "$ROOT_DIR" && "$DRIVER" "$WORK_REL/$stem.pgy" --emit-c-verified) \
        >"$WORK_DIR/$stem.c" 2>"$WORK_DIR/$stem.err" || fail "$stem source C failed"
    "$CC" -x c -std=c11 -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread \
        "$WORK_DIR/$stem.c" -lm -o "$WORK_DIR/$stem.exe" >/dev/null \
        2>"$WORK_DIR/$stem.compile.err" || fail "$stem C did not compile"
    "$WORK_DIR/$stem.exe" | tr -d '\r' >"$WORK_DIR/$stem.run"
    (cd "$ROOT_DIR" && PGY_NATIVE_PIPELINE=1 "$PGY" "$WORK_REL/$stem.pgy" --backend=c \
        -o "$WORK_REL/$stem.native.exe") >/dev/null 2>&1 || fail "$stem native build failed"
    "$WORK_DIR/$stem.native.exe" | tr -d '\r' >"$WORK_DIR/$stem.native.run"
    cmp -s "$WORK_DIR/$stem.run" "$WORK_DIR/$stem.native.run" || fail "$stem runs unlike native"
done
sites="$(grep -o 'pgy_region_string_concat(&__pgy_region' "$WORK_DIR/region_calls.c" | wc -l | tr -d ' ')"
creates="$(grep -c 'PgyRegion __pgy_region = pgy_region_create(0);' "$WORK_DIR/region_calls.c" || true)"
# Describe: two `+` of the Log spine, Print, StringLength, Sink, early Log;
# Main: the closing Log.
[[ "$sites" == 7 ]] || fail "region sites: $sites, expected 7"
[[ "$creates" == 2 ]] || fail "region scopes: $creates, expected 2 (Describe, Main)"
destroys="$(grep -c 'pgy_region_destroy(&__pgy_region);' "$WORK_DIR/region_calls.c" || true)"
(( destroys >= 4 )) || fail "region frees: $destroys, expected one per exit"
"$CC" -x c -std=c11 -g -fsanitize=address -fno-omit-frame-pointer \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread "$WORK_DIR/region_calls.c" -lm \
    -o "$WORK_DIR/region_calls.asan" >/dev/null 2>"$WORK_DIR/region_calls.asan.compile.err" ||
    fail "region_calls did not compile under AddressSanitizer"
ASAN_OPTIONS=detect_leaks=1 "$WORK_DIR/region_calls.asan" >/dev/null \
    2>"$WORK_DIR/region_calls.asan.err" || fail "region_calls leaked or failed under LeakSanitizer"

# heap_escape: nothing that outlives its call comes from a region.
! grep -Fq 'pgy_region' "$WORK_DIR/heap_escape.c" ||
    fail "an escaping concatenation was allocated in a region"
# Direct-MIR C and LLVM from the self-host MIR of every program. LLVM links
# the runtime object for the panic exports Array index and UnwrapOption reach.
"$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$WORK_DIR/runtime.o" ||
    fail "runtime object did not compile"
for stem in $STEMS; do
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$WORK_REL/$stem.pgy" \
        -o "$WORK_REL/$stem.mir.json") >/dev/null 2>"$WORK_DIR/$stem.mir.err" ||
        fail "$stem MIR production failed"
    for backend in c llvm; do
        suffix=c; [[ "$backend" == llvm ]] && suffix=ll
        (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$WORK_REL/$stem.mir.json" \
            -o "$WORK_REL/$stem.direct.$suffix") >"$WORK_DIR/$stem.direct-$backend.out" 2>&1 ||
            fail "$stem direct $backend failed"
    done
    "$CC" -x c -std=c11 -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread \
        "$WORK_DIR/$stem.direct.c" -lm -o "$WORK_DIR/$stem.direct-c.exe" >/dev/null \
        2>"$WORK_DIR/$stem.direct-c.compile.err" || fail "$stem direct C did not compile"
    "$WORK_DIR/$stem.direct-c.exe" | tr -d '\r' >"$WORK_DIR/$stem.direct-c.run"
    cmp -s "$WORK_DIR/$stem.direct-c.run" "$WORK_DIR/$stem.native.run" || fail "$stem direct C runs unlike native"
    "$CLANG" -x ir "$WORK_DIR/$stem.direct.ll" -x none "$WORK_DIR/runtime.o" -pthread -lm \
        -o "$WORK_DIR/$stem.direct-llvm.exe" >/dev/null 2>"$WORK_DIR/$stem.direct-llvm.compile.err" ||
        fail "$stem direct LLVM did not compile"
    "$WORK_DIR/$stem.direct-llvm.exe" | tr -d '\r' >"$WORK_DIR/$stem.direct-llvm.run"
    cmp -s "$WORK_DIR/$stem.direct-llvm.run" "$WORK_DIR/$stem.native.run" || fail "$stem direct LLVM runs unlike native"
done
for stem in region_calls region_try; do
    grep -Fq 'pgy_region_string_concat(' "$WORK_DIR/$stem.direct.c" || fail "$stem direct C allocated no region site"
    grep -Fq 'PgyRegion __pgy_region = pgy_region_create(0);' "$WORK_DIR/$stem.direct.c" || fail "$stem direct C opened no routine region"
    grep -Fq 'call ptr @pgy.region.concat(ptr %pgy.region' "$WORK_DIR/$stem.direct.ll" || fail "$stem direct LLVM allocated no region site"
    # Every exit, the `?` early return included, frees the region: no leak.
    "$CC" -x c -std=c11 -g -fsanitize=address -fno-omit-frame-pointer \
        -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread "$WORK_DIR/$stem.direct.c" -lm \
        -o "$WORK_DIR/$stem.direct.asan" >/dev/null 2>&1 || fail "$stem direct C did not compile under AddressSanitizer"
    ASAN_OPTIONS=detect_leaks=1 "$WORK_DIR/$stem.direct.asan" >/dev/null \
        2>"$WORK_DIR/$stem.direct.asan.err" || fail "$stem direct C leaked or failed under LeakSanitizer"
    "$CLANG" -fsanitize=address -x ir "$WORK_DIR/$stem.direct.ll" -x none "$WORK_DIR/runtime.o" -pthread -lm \
        -o "$WORK_DIR/$stem.direct-llvm.asan" >/dev/null 2>&1 || fail "$stem direct LLVM did not link under AddressSanitizer"
    ASAN_OPTIONS=detect_leaks=1 "$WORK_DIR/$stem.direct-llvm.asan" >/dev/null \
        2>"$WORK_DIR/$stem.direct-llvm.asan.err" || fail "$stem direct LLVM leaked or failed under LeakSanitizer"
done
! grep -Fq 'pgy_region' "$WORK_DIR/heap_escape.direct.c" || fail "direct C put an escaping concatenation in a region"
! grep -Fq 'pgy.region' "$WORK_DIR/heap_escape.direct.ll" || fail "direct LLVM put an escaping concatenation in a region"

# Forged sites: a leaf listed as a site, a malformed list, an index past the graph.
"$PYTHON_BIN" - "$WORK_DIR/region_calls.mir.json" "$WORK_DIR" <<'PY'
import json, sys
src, out = sys.argv[1:]
doc = json.load(open(src, encoding="utf-8"))
def first_site(d):
    for r in d["routines"]:
        for b in r["blocks"]:
            for i in b["instructions"]:
                if i.get("expr0_region_sites"):
                    return i
    raise SystemExit("no expr0_region_sites in the self-host MIR")
for how in ("leaf", "malformed", "window"):
    d = json.loads(json.dumps(doc))
    inst = first_site(d)
    nodes = inst["expr0_graph"]["nodes"]
    if how == "leaf":
        leaf = next(k for k, n in enumerate(nodes) if n["kind"] != "add")
        inst["expr0_region_sites"] = [leaf]
    elif how == "malformed":
        inst["expr0_region_sites"] = "4"
    else:
        inst["expr0_region_sites"] = [len(nodes) + 3]
    open("%s/forged_%s.mir.json" % (out, how), "w", encoding="utf-8").write(json.dumps(d, separators=(",", ":")))
PY
for how in leaf malformed window; do
    for backend in c llvm; do
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$WORK_REL/forged_$how.mir.json" \
            -o "$WORK_REL/forged_$how.$backend.out") >"$WORK_DIR/forged_$how.$backend.log" 2>&1; then
            fail "direct $backend accepted a forged region site ($how)"
        fi
        grep -Fq 'region-site' "$WORK_DIR/forged_$how.$backend.log" ||
            fail "direct $backend refused the forged region site ($how) for another reason"
    done
done
echo "[$LABEL] 7 call-duration concatenations in 2 function regions on source C, and on direct C and LLVM from the MIR sites, freed on every exit (the \`?\` return too) and leak-free; escaping concatenations stay on the heap; 3 forged sites refused"

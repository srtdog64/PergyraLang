#!/usr/bin/env bash
# Self-host region allocation on the installed source C route
# (semantic/region_escape_site_owner.pgy, codegen region_scope_emit_owner.pgy).
# A String `+` spine given straight to a call-duration consumer comes from the
# function region, which every return frees: the programs run as native does
# and LeakSanitizer reports nothing. A concatenation that outlives the call
# (bound, returned, stored) stays on the heap: its C names no region.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-region-allocation"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
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

# region_calls: the region carries its sites, every exit frees it, no leak.
for stem in region_calls heap_escape; do
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
echo "[$LABEL] 7 call-duration concatenations in 2 function regions, freed on every exit and leak-free; bound, returned and stored concatenations stay on the heap"

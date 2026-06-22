#!/usr/bin/env bash
# MIR-JSON self-host coverage probe (companion to mir_json_parity.sh).
#
# The parity gate proves the committed fixture set passes the self-host lowering
# path (pgy --mir-json | mir_lower | codegen | gcc == C oracle). This probe maps
# the BOUNDARY of that path: it runs a spread of language constructs through the
# same pipeline and reports, per construct, exactly where it stands --
#
#   PASS            full pipeline matches the C oracle
#   MIR-LOWER-gap   mir_lower (src/self_hosted/mir_lower/main.pgy) rejects it
#   CODEGEN-gap     codegen   (src/self_hosted/codegen/main.pgy)   rejects it
#   CC-fail         reconstructed C does not compile
#   STDOUT-diff     runs, but output differs from the oracle
#   ORACLE-skip     the C oracle itself could not build/run it
#
# This is a coverage probe, not a gate. It is the "empty parts" map for the
# self-hosted SOT lowering subset. Read-only; writes only under .tmp.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "missing pgy: $PGY" >&2; exit 1; }
CC="${CC:-gcc}"

MIR_LOWER_SRC="$ROOT_DIR/src/self_hosted/mir_lower/main.pgy"
CODEGEN_SRC="$ROOT_DIR/src/self_hosted/codegen/main.pgy"
B="$ROOT_DIR/.tmp/self_hosted/mir_lower/coverage"
mkdir -p "$B"

echo "[coverage] building gen0 mir_lower + codegen..."
rm -f "$B/mir_lower.exe" "$B/codegen.exe"
(cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$MIR_LOWER_SRC")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$B/mir_lower.exe")" >/dev/null)
(cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$CODEGEN_SRC")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$B/codegen.exe")" >/dev/null)
[[ -x "$B/mir_lower.exe" && -x "$B/codegen.exe" ]] || {
    echo "[coverage] failed to build gen0 self-host tools" >&2
    exit 1
}

# One probe construct per call: classify(name, source).
classify() {
    local name="$1" src_file="$2"
    local mj="$B/$name.mirjson" reast="$B/$name.reast" via_c="$B/$name.c"
    local rel="${src_file#$ROOT_DIR/}"

    local orc_rc orc
    (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$src_file")" --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$B/${name}_oracle.exe")" >/dev/null 2>&1)
    orc_rc=$?
    if [[ $orc_rc -ne 0 ]]; then printf '  %-18s ORACLE-skip\n' "$name"; return; fi

    (cd "$ROOT_DIR" && "$PGY" --mir-json "$(pgy_path_for_compiler "$PGY" "$src_file")" \
        2>/dev/null | tr -d '\r' > "$mj")
    if ! grep -q '"schema":"pgy.mir.v1"' "$mj"; then printf '  %-18s MIRJSON-skip\n' "$name"; return; fi

    "$B/mir_lower.exe" "${mj#$ROOT_DIR/}" 2>/dev/null | tr -d '\r' > "$reast" || true
    if grep -q '^MIR-LOWER ERROR' "$reast"; then
        printf '  %-18s MIR-LOWER-gap   %s\n' "$name" "$(grep -m1 '^MIR-LOWER ERROR' "$reast" | cut -c1-60)"; return
    fi
    # mir_lower can reconstruct a *flat* AST that drops control-flow structure
    # (the if/while/for becomes a bare condition + flattened bodies). codegen
    # supports `If:`/`While:`/`For:` (confirmed), so a downstream codegen error
    # here is really mir_lower's missing CFG -> structured-AST reconstruction.
    if grep -qE '^[[:space:]]*(if|while|for)[[:space:]]' "$src_file" \
        && ! grep -qE 'If:|While:|For: ' "$reast"; then
        printf '  %-18s MIR-LOWER-flatten  (codegen ready; mir_lower dropped control-flow structure)\n' "$name"; return
    fi
    # Multiple source functions but fewer reconstructed Function: nodes -> mir_lower
    # merged/dropped functions (codegen supports multiple functions).
    local src_funcs reast_funcs
    src_funcs=$(grep -cE '^func ' "$src_file")
    reast_funcs=$(grep -cE '^  Function: ' "$reast")
    if [[ "$src_funcs" -gt 1 && "$reast_funcs" -lt "$src_funcs" ]]; then
        printf '  %-18s MIR-LOWER-merge    (codegen ready; mir_lower merged/dropped functions)\n' "$name"; return
    fi
    "$B/codegen.exe" "${reast#$ROOT_DIR/}" 2>/dev/null | tr -d '\r' > "$via_c" || true
    if grep -q '^CODEGEN ERROR' "$via_c"; then
        printf '  %-18s CODEGEN-gap     %s\n' "$name" "$(grep -m1 '^CODEGEN ERROR' "$via_c" | cut -c1-60)"; return
    fi
    if ! "$CC" "$via_c" -o "$B/${name}_via.exe" 2>"$B/${name}_cc.log"; then
        printf '  %-18s CC-fail\n' "$name"; return
    fi
    via="$(cd "$ROOT_DIR" && "$B/${name}_via.exe" 2>/dev/null | tr -d '\r')"
    orc="$(cd "$ROOT_DIR" && "$B/${name}_oracle.exe" 2>/dev/null | tr -d '\r')"
    if [[ "$via" == "$orc" ]]; then printf '  %-18s PASS\n' "$name"
    else printf '  %-18s STDOUT-diff\n' "$name"; fi
}

probe() { local name="$1"; local f="$B/src_$name.pgy"; cat > "$f"; classify "$name" "$f"; }

echo "[coverage] self-host MIR->C lowering subset boundary:"

probe if_else <<'EOF'
func Main() -> Void {
    let x: Int = 3;
    if x > 0 { Log("pos"); } else { Log("neg"); }
}
EOF
probe while_loop <<'EOF'
func Main() -> Void {
    let i: Int = 0;
    while i < 3 { Log(ToString(i)); i = i + 1; }
}
EOF
probe for_loop <<'EOF'
func Main() -> Void {
    for i in 0..3 { Log(ToString(i)); }
}
EOF
probe func_param <<'EOF'
func Add(a: Int, b: Int) -> Int { return a + b; }
func Main() -> Void { Log(ToString(Add(2, 5))); }
EOF
probe bool_ops <<'EOF'
func Main() -> Void {
    let a: Bool = true;
    let b: Bool = false;
    if a && (b == false) { Log("y"); }
}
EOF
probe nested_if <<'EOF'
func Main() -> Void {
    let x: Int = 5;
    if x > 0 { if x > 3 { Log("big"); } }
}
EOF
probe reassign_block <<'EOF'
func Main() -> Void {
    let x: Int = 0;
    if true { x = 10; }
    Log(ToString(x));
}
EOF
probe string_concat <<'EOF'
func Main() -> Void {
    let a: String = "he";
    let b: String = "llo";
    Log(Concat(a, b));
}
EOF
probe multi_func_void <<'EOF'
func Greet() -> Void { Log("hi"); }
func Bye() -> Void { Log("bye"); }
func Main() -> Void { Greet(); Bye(); }
EOF

echo "[coverage] done. PASS = covered; *-gap = the empty parts (stage attributed)."

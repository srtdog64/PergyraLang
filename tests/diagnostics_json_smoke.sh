#!/usr/bin/env bash
# diagnostics_json_smoke.sh
#
# Regression: verify `pgy --error-format=json` emits machine-readable
# JSON on stderr. Covers representative paths:
#   (1) semantic errors with stage/code/cause_ir/fix_source
#   (2) parse errors routed as stage="parse"
#   (3) lex errors routed as stage="lex"
#   (4) AIR strict-evidence errors routed through semantic JSON
#   (5) runtime/spec-limit errors with stable codes
#   (6) success path → empty array "[]"
#
# Structured-tooling consumers rely on this being parseable without
# falling back to regex on free-text messages. Shape is tight; breaking
# it should fail this smoke test.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[diag-json] missing compiler binary: $PGY" >&2
    exit 1
fi

# Ensure LLVM runtime DLLs are on PATH when running under MinGW/MSYS
# so pgy.exe can resolve LLVM-C.dll. Same pattern as compare_backends.sh.
case "$(uname -s 2>/dev/null || echo unknown)" in
    MINGW*|MSYS*|CYGWIN*)
        for candidate in \
            "/c/Program Files/LLVM/bin" \
            "/c/LLVM/bin" \
            "${MSYSTEM_PREFIX:-}/bin" \
            "${LLVM_INSTALL:-}/bin"; do
            [[ -d "$candidate" ]] || continue
            case ":${PATH}:" in
                *":$candidate:"*) ;;
                *) PATH="$candidate:$PATH" ;;
            esac
        done
        ;;
esac

TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_diag_json.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

PY_BIN=""
if command -v python3 >/dev/null 2>&1; then
    PY_BIN="$(command -v python3)"
elif command -v python >/dev/null 2>&1; then
    PY_BIN="$(command -v python)"
fi

check_json() {
    local label="$1"
    local file="$2"
    local expect_expr="$3"  # python expression over `data`, must be True

    if [[ -z "$PY_BIN" ]]; then
        echo "[diag-json] $label: SKIP (no python)" >&2
        return 0
    fi
    "$PY_BIN" - "$file" "$label" "$expect_expr" <<'PY'
import json, sys
path, label, expr = sys.argv[1], sys.argv[2], sys.argv[3]
with open(path, "r", encoding="utf-8") as fh:
    raw = fh.read().strip()
try:
    data = json.loads(raw)
except json.JSONDecodeError as e:
    print(f"[diag-json] {label}: FAIL — not valid JSON: {e}", file=sys.stderr)
    print(f"  raw: {raw[:200]!r}", file=sys.stderr)
    sys.exit(1)
ok = eval(expr, {"data": data, "all": all, "any": any, "len": len})
if not ok:
    print(f"[diag-json] {label}: FAIL — assertion {expr} false", file=sys.stderr)
    print(f"  data: {data}", file=sys.stderr)
    sys.exit(1)
print(f"[diag-json] {label}: OK")
PY
}

# --- case 1: semantic error ---
SEM_SRC="$WORK_DIR/sem.pgy"
cat > "$SEM_SRC" <<'EOF'
func Main() -> Void {
    let x: Int = "not an int";
}
EOF
SEM_ERR="$WORK_DIR/sem.err"
if "$PGY" "$SEM_SRC" --backend=c --error-format=json 2>"$SEM_ERR"; then
    echo "[diag-json] semantic: FAIL — expected non-zero exit" >&2
    cat "$SEM_ERR" >&2
    exit 1
fi
check_json "semantic" "$SEM_ERR" \
  'isinstance(data, list) and len(data) >= 1 and data[0].get("severity") == "error" and data[0].get("stage") == "semantic" and "location" in data[0] and "message" in data[0]'
# Type-mismatch sites now carry a stable PGY_SEM_* code — tooling routes on this.
check_json "semantic-code" "$SEM_ERR" \
  'isinstance(data, list) and data[0].get("code") == "PGY_SEM_TYPE_MISMATCH"'
# Routing hints — cause_ir (IR-level origin) + fix_source (source-level
# repair action token). Stable across versions; meaning frozen once shipped.
check_json "semantic-hints" "$SEM_ERR" \
  'isinstance(data, list) and data[0].get("cause_ir") == "semantic:assignability_check" and data[0].get("fix_source") == "annotate-or-convert"'

# --- case 2: parse error ---
PARSE_SRC="$WORK_DIR/parse.pgy"
cat > "$PARSE_SRC" <<'EOF'
func Main() -> Void {
    let x: Int = ;
}
EOF
PARSE_ERR="$WORK_DIR/parse.err"
if "$PGY" "$PARSE_SRC" --backend=c --error-format=json 2>"$PARSE_ERR"; then
    echo "[diag-json] parse: FAIL — expected non-zero exit" >&2
    cat "$PARSE_ERR" >&2
    exit 1
fi
check_json "parse" "$PARSE_ERR" \
  'isinstance(data, list) and len(data) == 1 and data[0].get("severity") == "error" and data[0].get("stage") == "parse"'
check_json "parse-code" "$PARSE_ERR" \
  'isinstance(data, list) and len(data) == 1 and data[0].get("code") == "PGY_PARSE_SYNTAX" and data[0].get("cause_ir") == "parse:unexpected_token" and data[0].get("fix_source") == "check-syntax"'

# --- case 2a: beta-out-of-scope pin syntax is explicitly rejected ---
PIN_SRC="$WORK_DIR/pin_syntax.pgy"
cat > "$PIN_SRC" <<'EOF'
func Main() -> Void {
    let scores: Slot<Int> = ClaimSlot<Int>();
    pin scores as view {
        Log(view);
    }
}
EOF
PIN_ERR="$WORK_DIR/pin_syntax.err"
if "$PGY" "$PIN_SRC" --backend=c --error-format=json 2>"$PIN_ERR"; then
    echo "[diag-json] pin-syntax: FAIL -- expected non-zero exit" >&2
    cat "$PIN_ERR" >&2
    exit 1
fi
check_json "pin-syntax-reject" "$PIN_ERR" \
  'isinstance(data, list) and len(data) == 1 and data[0].get("stage") == "parse" and data[0].get("code") == "PGY_PARSE_SYNTAX" and "Pin/Lease syntax is not beta-stable yet" in data[0].get("message", "")'

# --- case 2a: lex error ---
LEX_SRC="$WORK_DIR/lex.pgy"
cat > "$LEX_SRC" <<'EOF'
func Main() -> Void {
    @
}
EOF
LEX_ERR="$WORK_DIR/lex.err"
if "$PGY" "$LEX_SRC" --backend=c --error-format=json 2>"$LEX_ERR"; then
    echo "[diag-json] lex: FAIL ??expected non-zero exit" >&2
    cat "$LEX_ERR" >&2
    exit 1
fi
check_json "lex" "$LEX_ERR" \
  'isinstance(data, list) and len(data) == 1 and data[0].get("severity") == "error" and data[0].get("stage") == "lex"'
check_json "lex-code" "$LEX_ERR" \
  'isinstance(data, list) and len(data) == 1 and data[0].get("code") == "PGY_LEX_INVALID_TOKEN" and data[0].get("cause_ir") == "lex:invalid_token" and data[0].get("fix_source") == "remove-or-escape-character"'

# --- case 2b: AIR strict evidence from parsed source ---
AIR_SRC="$WORK_DIR/air_missing_authority.pgy"
cat > "$AIR_SRC" <<'EOF'
subject Buyer { let hp: Int; action Pay(self) -> Void { return; } }
ability Payable { func Pay() -> Void; }
role BuyerPay for Buyer {
    impl ability Payable { func Pay() -> Void { return; } }
}
effect PaymentEffect for bearer: Buyer { }
zone PaymentZone {
    subject slot buyer: Buyer
    effect slot paymentFx: PaymentEffect
}
intent Purchase(payment: PaymentZone, buyer: Buyer) {
    step pay {
        where: PaymentZone;
        using: payment;
        who: buyer;
        requires: Payable;
        authorized by: buyer;
        causes: PaymentEffect;
    }
}
func Main() -> Void { }
EOF
AIR_ERR="$WORK_DIR/air_missing_authority.err"
if "$PGY" "$AIR_SRC" --backend=c --error-format=json 2>"$AIR_ERR"; then
    echo "[diag-json] air: FAIL -- expected non-zero exit" >&2
    cat "$AIR_ERR" >&2
    exit 1
fi
check_json "air-evidence" "$AIR_ERR" \
  'isinstance(data, list) and len(data) == 1 and data[0].get("stage") == "semantic" and data[0].get("code") == "PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING" and data[0].get("location", {}).get("line", 0) > 0 and data[0].get("location", {}).get("column", 0) > 0'
check_json "air-evidence-hints" "$AIR_ERR" \
  'isinstance(data, list) and data[0].get("cause_ir") == "semantic:intent:boundary_evidence" and data[0].get("fix_source") == "align-intent-boundary-evidence" and "expected authority participant(s): buyer" in data[0].get("message", "")'

# --- case 2c: slot lifecycle code routes through ---
SLOT_SRC="$WORK_DIR/slot.pgy"
cat > "$SLOT_SRC" <<'EOF'
func Main() -> Void {
    let slot: Slot<Int> = ClaimSlot<Int>();
    Write(slot, 42);
    Release(slot);
    let v: Int = Read(slot);
    Log(ToString(v));
}
EOF
SLOT_ERR="$WORK_DIR/slot.err"
if "$PGY" "$SLOT_SRC" --backend=c --error-format=json 2>"$SLOT_ERR"; then
    echo "[diag-json] slot-released: FAIL — expected non-zero exit" >&2
    cat "$SLOT_ERR" >&2
    exit 1
fi
check_json "slot-released-code" "$SLOT_ERR" \
  'isinstance(data, list) and any(d.get("code") == "PGY_SEM_SLOT_RELEASED" for d in data)'

# --- case 2d: payload-bearing ownership diagnostic exports structured payload ---
OWN_SRC="$WORK_DIR/own_payload.pgy"
cat > "$OWN_SRC" <<'EOF'
class Packet {
    let id: Int;
}

class Wrapper {
    let packet: Packet;
}

class Cargo {
    let wrapper: Wrapper;
}

func Leak(ref cargo: Cargo) -> Void {
    let alias = cargo.wrapper.packet;
    Log(alias.id);
}
EOF
OWN_ERR="$WORK_DIR/own_payload.err"
if "$PGY" "$OWN_SRC" --backend=c --error-format=json 2>"$OWN_ERR"; then
    echo "[diag-json] own-payload: FAIL — expected non-zero exit" >&2
    cat "$OWN_ERR" >&2
    exit 1
fi
check_json "own-payload" "$OWN_ERR" \
  'isinstance(data, list) and any(d.get("code") == "PGY_SEM_BORROW_ESCAPE" and isinstance(d.get("payload"), dict) and d["payload"].get("borrowed_name") == "cargo" and d["payload"].get("consumer_name") == "Leak" for d in data)'

# --- case 2c: LLVM spec limit → stage=llvm_codegen + PGY_LLVM_SPEC_LIMIT ---
# Only runs if the LLVM backend is available. 33 distinct Result<Int, E> error
# enums overflow MAX_LLVM_RESULT_SPECS=32. The error surfaces from inside LLVM
# codegen (before native link), so stage must be "llvm_codegen", not
# "backend_llvm_native".
SPEC_SRC="$WORK_DIR/spec.pgy"
{
    for i in $(seq 1 33); do echo "enum E${i} { A }"; done
    for i in $(seq 1 33); do
        echo "func f${i}(x: Int) -> Result<Int, E${i}> { return Ok(x); }"
    done
    echo 'func Main() -> Void { Log("ok"); }'
} > "$SPEC_SRC"
SPEC_ERR="$WORK_DIR/spec.err"
SPEC_OBJ="$WORK_DIR/spec_obj"
if "$PGY" "$SPEC_SRC" --backend=llvm --error-format=json -o "$SPEC_OBJ" \
        >/dev/null 2>"$SPEC_ERR"; then
    # If the LLVM backend isn't wired or the spec limit was raised, skip.
    echo "[diag-json] spec-limit: SKIP (backend did not exercise MAX_LLVM_RESULT_SPECS)"
else
    check_json "spec-limit" "$SPEC_ERR" \
      'isinstance(data, list) and any(d.get("code") == "PGY_LLVM_SPEC_LIMIT" and d.get("stage") == "llvm_codegen" for d in data)'
    # Backend hints propagate through LLVMGenCtx → LLVMGenResult →
    # CompilerResult → driver_emit_single_diag_json_full.
    check_json "spec-limit-hints" "$SPEC_ERR" \
      'isinstance(data, list) and any(d.get("cause_ir") == "llvm:result_spec:capacity_exceeded" and d.get("fix_source") == "reuse-shared-error-enum" for d in data)'
fi

# --- case 3: success path emits [] ---
OK_SRC="$WORK_DIR/ok.pgy"
cat > "$OK_SRC" <<'EOF'
func Main() -> Void {
    Log("hi");
}
EOF
OK_OUT="$WORK_DIR/ok.out"
OK_ERR="$WORK_DIR/ok.err"
OK_BIN="$WORK_DIR/ok_bin"
if ! "$PGY" "$OK_SRC" --backend=c --error-format=json -o "$OK_BIN" \
        >"$OK_OUT" 2>"$OK_ERR"; then
    echo "[diag-json] ok: FAIL — expected zero exit" >&2
    cat "$OK_ERR" >&2
    exit 1
fi
check_json "ok-empty" "$OK_ERR" 'isinstance(data, list) and len(data) == 0'

echo "[diag-json] all cases pass"

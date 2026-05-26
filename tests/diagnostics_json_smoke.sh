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
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[diag-json] missing compiler binary: $PGY" >&2
    exit 1
fi

# Ensure MinGW/LLVM runtime DLLs are before stale PATH entries when running
# Windows pgy.exe from Git Bash/MSYS.
pgy_prepend_windows_runtime_paths

pgy_arg_for_compiler() {
    case "$1" in
        /*) pgy_path_for_compiler "$PGY" "$1" ;;
        *) printf '%s\n' "$1" ;;
    esac
}

pgy_run() {
    local args=()
    local arg
    for arg in "$@"; do
        args+=("$(pgy_arg_for_compiler "$arg")")
    done
    "$PGY" "${args[@]}"
}

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
        local compact
        compact="$(tr -d '\r\n\t ' < "$file")"
        if [[ -z "$compact" || "${compact:0:1}" != "[" || "${compact: -1}" != "]" ]]; then
            echo "[diag-json] $label: FAIL -- stderr is not a JSON array" >&2
            sed -n '1,8p' "$file" >&2
            exit 1
        fi
        if [[ "$label" == "ok-empty" && "$compact" != "[]" ]]; then
            echo "[diag-json] $label: FAIL -- expected empty diagnostic array" >&2
            sed -n '1,8p' "$file" >&2
            exit 1
        fi
        if [[ "$label" == "llvm-channel-runtime-missing" ]]; then
            if ! grep -Fq -- "pgy_channel_init_Bool" "$file" \
                && ! grep -Fq -- "pgy_channel_try_recv_Bool" "$file"; then
                echo "[diag-json] $label: FAIL -- missing expected channel helper name" >&2
                sed -n '1,8p' "$file" >&2
                exit 1
            fi
        fi
        while IFS= read -r literal; do
            [[ -n "$literal" ]] || continue
            if [[ "$label" == "llvm-channel-runtime-missing" \
                && ( "$literal" == "pgy_channel_init_Bool" \
                     || "$literal" == "pgy_channel_try_recv_Bool" ) ]]; then
                continue
            fi
            if ! grep -Fq -- "$literal" "$file"; then
                echo "[diag-json] $label: FAIL -- missing expected JSON literal: $literal" >&2
                sed -n '1,8p' "$file" >&2
                exit 1
            fi
        done < <(
            printf '%s\n' "$expect_expr" |
                grep -oE '"([^"\\]|\\.)*"' |
                sed -E 's/^"(.*)"$/\1/'
        )
        echo "[diag-json] $label: OK (shell fallback)"
        return 0
    fi
    "$PY_BIN" - "$file" "$label" "$expect_expr" <<'PY'
import json, sys
path, label, expr = sys.argv[1], sys.argv[2], sys.argv[3]
with open(path, "r", encoding="utf-8") as fh:
    raw = fh.read().strip()
start = raw.find("[")
end = raw.rfind("]")
if start > 0 and end >= start:
    raw = raw[start:end + 1]
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
if pgy_run "$SEM_SRC" --backend=c --error-format=json 2>"$SEM_ERR"; then
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
  'isinstance(data, list) and data[0].get("cause_ir") == "semantic:assignability_check" and data[0].get("fix_source") == "annotate-or-convert" and data[0].get("layer") == "type"'

# --- case 2: parse error ---
PARSE_SRC="$WORK_DIR/parse.pgy"
cat > "$PARSE_SRC" <<'EOF'
func Main() -> Void {
    let x: Int = ;
}
EOF
PARSE_ERR="$WORK_DIR/parse.err"
if pgy_run "$PARSE_SRC" --backend=c --error-format=json 2>"$PARSE_ERR"; then
    echo "[diag-json] parse: FAIL — expected non-zero exit" >&2
    cat "$PARSE_ERR" >&2
    exit 1
fi
check_json "parse" "$PARSE_ERR" \
  'isinstance(data, list) and len(data) == 1 and data[0].get("severity") == "error" and data[0].get("stage") == "parse" and data[0].get("layer") == "syntax"'
check_json "parse-code" "$PARSE_ERR" \
  'isinstance(data, list) and len(data) == 1 and data[0].get("code") == "PGY_PARSE_SYNTAX" and data[0].get("cause_ir") == "parse:unexpected_token" and data[0].get("fix_source") == "check-syntax"'

# --- case 2a: source-level pin syntax reaches semantic pin diagnostics ---
PIN_SRC="$WORK_DIR/pin_source_await_boundary.pgy"
cat > "$PIN_SRC" <<'EOF'
async func Main() -> Void {
    let scores: Slot<Int> = ClaimSlot();
    pin scores as view: ReadView<Int> {
        let pending = spawn 42;
        let value = await pending;
        Log(value);
    }
}
EOF
PIN_ERR="$WORK_DIR/pin_syntax.err"
if pgy_run "$PIN_SRC" --backend=c --error-format=json 2>"$PIN_ERR"; then
    echo "[diag-json] pin-source-await-boundary: FAIL -- expected non-zero exit" >&2
    cat "$PIN_ERR" >&2
    exit 1
fi
check_json "pin-source-await-boundary" "$PIN_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("layer") == "resource" and d.get("code") == "PGY_SEM_PIN_AWAIT_BOUNDARY" and d.get("cause_ir") == "semantic:pin:await_boundary" and d.get("fix_source") == "end-pin-before-await" and "await suspension boundary" in d.get("message", "") for d in data)'

# --- case 2b: stable view surface uses pin semantic diagnostics ---
PIN_RETURN_SRC="$WORK_DIR/pin_return_escape.pgy"
cat > "$PIN_RETURN_SRC" <<'EOF'
func Leak() -> ReadView<Int> {
    let scores: Slot<Int> = ClaimSlot();
    let view: ReadView<Int> = ViewRead(scores);
    return view;
}
func Main() -> Void {
    Log(1);
}
EOF
PIN_RETURN_ERR="$WORK_DIR/pin_return_escape.err"
if pgy_run "$PIN_RETURN_SRC" --backend=c --error-format=json 2>"$PIN_RETURN_ERR"; then
    echo "[diag-json] pin-return-escape: FAIL -- expected non-zero exit" >&2
    cat "$PIN_RETURN_ERR" >&2
    exit 1
fi
check_json "pin-return-escape" "$PIN_RETURN_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_ESCAPE" and d.get("cause_ir") == "semantic:pin:escape" and d.get("fix_source") == "change-ref-to-own-or-stop-escape" for d in data)'

PIN_PARALLEL_SRC="$WORK_DIR/pin_parallel_boundary.pgy"
cat > "$PIN_PARALLEL_SRC" <<'EOF'
func Main() -> Void {
    let scores: Slot<Int> = ClaimSlot();
    let view: ReadView<Int> = ViewRead(scores);
    parallel {
        Log(1);
    }
}
EOF
PIN_PARALLEL_ERR="$WORK_DIR/pin_parallel_boundary.err"
if pgy_run "$PIN_PARALLEL_SRC" --backend=c --error-format=json 2>"$PIN_PARALLEL_ERR"; then
    echo "[diag-json] pin-parallel-boundary: FAIL -- expected non-zero exit" >&2
    cat "$PIN_PARALLEL_ERR" >&2
    exit 1
fi
check_json "pin-parallel-boundary" "$PIN_PARALLEL_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_PARALLEL_CONFLICT" and d.get("cause_ir") == "semantic:pin:parallel_conflict" and d.get("fix_source") == "serialize-pin-access" for d in data)'

PIN_WRITE_EXCLUSIVE_SRC="$WORK_DIR/pin_write_exclusive.pgy"
cat > "$PIN_WRITE_EXCLUSIVE_SRC" <<'EOF'
func Main() -> Void {
    let scores: Slot<Int> = ClaimSlot();
    let read: ReadView<Int> = ViewRead(scores);
    let write: WriteView<Int> = ViewWrite(scores);
}
EOF
PIN_WRITE_EXCLUSIVE_ERR="$WORK_DIR/pin_write_exclusive.err"
if pgy_run "$PIN_WRITE_EXCLUSIVE_SRC" --backend=c --error-format=json 2>"$PIN_WRITE_EXCLUSIVE_ERR"; then
    echo "[diag-json] pin-write-exclusive: FAIL -- expected non-zero exit" >&2
    cat "$PIN_WRITE_EXCLUSIVE_ERR" >&2
    exit 1
fi
check_json "pin-write-exclusive" "$PIN_WRITE_EXCLUSIVE_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_PARALLEL_CONFLICT" and d.get("cause_ir") == "semantic:pin:parallel_conflict" and d.get("fix_source") == "serialize-pin-access" and "WriteView<T> is exclusive" in d.get("message", "") for d in data)'

PIN_RELEASE_ACTIVE_SRC="$WORK_DIR/pin_release_active_view.pgy"
cat > "$PIN_RELEASE_ACTIVE_SRC" <<'EOF'
func Main() -> Void {
    let scores: Slot<Int> = ClaimSlot();
    pin scores as view: ReadView<Int> {
        Release(scores);
    }
}
EOF
PIN_RELEASE_ACTIVE_ERR="$WORK_DIR/pin_release_active_view.err"
if pgy_run "$PIN_RELEASE_ACTIVE_SRC" --backend=c --error-format=json 2>"$PIN_RELEASE_ACTIVE_ERR"; then
    echo "[diag-json] pin-release-active-view: FAIL -- expected non-zero exit" >&2
    cat "$PIN_RELEASE_ACTIVE_ERR" >&2
    exit 1
fi
check_json "pin-release-active-view" "$PIN_RELEASE_ACTIVE_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_PARALLEL_CONFLICT" and d.get("cause_ir") == "semantic:pin:parallel_conflict" and d.get("fix_source") == "serialize-pin-access" and "Cannot release slot" in d.get("message", "") and "while ReadView" in d.get("message", "") for d in data)'

PIN_MOVE_ACTIVE_SRC="$WORK_DIR/pin_move_active_view.pgy"
cat > "$PIN_MOVE_ACTIVE_SRC" <<'EOF'
func Main() -> Void {
    let scores: Slot<Int> = ClaimSlot();
    pin scores as view: ReadView<Int> {
        let moved: MoveToken<Int> = Move(scores);
    }
}
EOF
PIN_MOVE_ACTIVE_ERR="$WORK_DIR/pin_move_active_view.err"
if pgy_run "$PIN_MOVE_ACTIVE_SRC" --backend=c --error-format=json 2>"$PIN_MOVE_ACTIVE_ERR"; then
    echo "[diag-json] pin-move-active-view: FAIL -- expected non-zero exit" >&2
    cat "$PIN_MOVE_ACTIVE_ERR" >&2
    exit 1
fi
check_json "pin-move-active-view" "$PIN_MOVE_ACTIVE_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_PARALLEL_CONFLICT" and d.get("cause_ir") == "semantic:pin:parallel_conflict" and d.get("fix_source") == "serialize-pin-access" and "Cannot move slot" in d.get("message", "") and "while ReadView" in d.get("message", "") for d in data)'

PIN_WRITE_OWNER_ACTIVE_SRC="$WORK_DIR/pin_write_owner_active_view.pgy"
cat > "$PIN_WRITE_OWNER_ACTIVE_SRC" <<'EOF'
func Main() -> Void {
    let scores: Slot<Int> = ClaimSlot();
    pin scores as view: ReadView<Int> {
        Write(scores, 1);
    }
}
EOF
PIN_WRITE_OWNER_ACTIVE_ERR="$WORK_DIR/pin_write_owner_active_view.err"
if pgy_run "$PIN_WRITE_OWNER_ACTIVE_SRC" --backend=c --error-format=json 2>"$PIN_WRITE_OWNER_ACTIVE_ERR"; then
    echo "[diag-json] pin-write-owner-active-view: FAIL -- expected non-zero exit" >&2
    cat "$PIN_WRITE_OWNER_ACTIVE_ERR" >&2
    exit 1
fi
check_json "pin-write-owner-active-view" "$PIN_WRITE_OWNER_ACTIVE_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_PARALLEL_CONFLICT" and d.get("cause_ir") == "semantic:pin:parallel_conflict" and d.get("fix_source") == "serialize-pin-access" and "Cannot write slot" in d.get("message", "") and "while ReadView" in d.get("message", "") for d in data)'

PIN_READ_OWNER_WRITE_VIEW_SRC="$WORK_DIR/pin_read_owner_write_view.pgy"
cat > "$PIN_READ_OWNER_WRITE_VIEW_SRC" <<'EOF'
func Main() -> Void {
    let scores: Slot<Int> = ClaimSlot();
    pin scores as view: WriteView<Int> {
        let value: Int = Read(scores);
    }
}
EOF
PIN_READ_OWNER_WRITE_VIEW_ERR="$WORK_DIR/pin_read_owner_write_view.err"
if pgy_run "$PIN_READ_OWNER_WRITE_VIEW_SRC" --backend=c --error-format=json 2>"$PIN_READ_OWNER_WRITE_VIEW_ERR"; then
    echo "[diag-json] pin-read-owner-write-view: FAIL -- expected non-zero exit" >&2
    cat "$PIN_READ_OWNER_WRITE_VIEW_ERR" >&2
    exit 1
fi
check_json "pin-read-owner-write-view" "$PIN_READ_OWNER_WRITE_VIEW_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_PARALLEL_CONFLICT" and d.get("cause_ir") == "semantic:pin:parallel_conflict" and d.get("fix_source") == "serialize-pin-access" and "Cannot read slot" in d.get("message", "") and "while WriteView" in d.get("message", "") for d in data)'

PIN_ASSIGN_SUGAR_ACTIVE_SRC="$WORK_DIR/pin_assign_sugar_active_view.pgy"
cat > "$PIN_ASSIGN_SUGAR_ACTIVE_SRC" <<'EOF'
func Main() -> Void {
    let scores: Slot<Int> = ClaimSlot();
    pin scores as view: ReadView<Int> {
        scores = 1;
    }
}
EOF
PIN_ASSIGN_SUGAR_ACTIVE_ERR="$WORK_DIR/pin_assign_sugar_active_view.err"
if pgy_run "$PIN_ASSIGN_SUGAR_ACTIVE_SRC" --backend=c --error-format=json 2>"$PIN_ASSIGN_SUGAR_ACTIVE_ERR"; then
    echo "[diag-json] pin-assign-sugar-active-view: FAIL -- expected non-zero exit" >&2
    cat "$PIN_ASSIGN_SUGAR_ACTIVE_ERR" >&2
    exit 1
fi
check_json "pin-assign-sugar-active-view" "$PIN_ASSIGN_SUGAR_ACTIVE_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_PARALLEL_CONFLICT" and d.get("cause_ir") == "semantic:pin:parallel_conflict" and d.get("fix_source") == "serialize-pin-access" and "Cannot assign to slot" in d.get("message", "") and "while ReadView" in d.get("message", "") for d in data)'

PIN_VALUE_SUGAR_ACTIVE_SRC="$WORK_DIR/pin_value_sugar_write_view.pgy"
cat > "$PIN_VALUE_SUGAR_ACTIVE_SRC" <<'EOF'
func Main() -> Void {
    let scores: Slot<Int> = ClaimSlot();
    Write(scores, 3);
    pin scores as view: WriteView<Int> {
        Log(scores);
    }
}
EOF
PIN_VALUE_SUGAR_ACTIVE_ERR="$WORK_DIR/pin_value_sugar_write_view.err"
if pgy_run "$PIN_VALUE_SUGAR_ACTIVE_SRC" --backend=c --error-format=json 2>"$PIN_VALUE_SUGAR_ACTIVE_ERR"; then
    echo "[diag-json] pin-value-sugar-write-view: FAIL -- expected non-zero exit" >&2
    cat "$PIN_VALUE_SUGAR_ACTIVE_ERR" >&2
    exit 1
fi
check_json "pin-value-sugar-write-view" "$PIN_VALUE_SUGAR_ACTIVE_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_PARALLEL_CONFLICT" and d.get("cause_ir") == "semantic:pin:parallel_conflict" and d.get("fix_source") == "serialize-pin-access" and "auto-reads the owner slot" in d.get("message", "") and "while WriteView" in d.get("message", "") for d in data)'

PIN_HELPER_OWNER_ACTIVE_SRC="$WORK_DIR/pin_helper_owner_active_view.pgy"
cat > "$PIN_HELPER_OWNER_ACTIVE_SRC" <<'EOF'
func Touch(ref s: Slot<Int>) -> Void {
    Write(s, 7);
}
func Main() -> Void {
    let scores: Slot<Int> = ClaimSlot();
    pin scores as view: ReadView<Int> {
        Touch(scores);
    }
}
EOF
PIN_HELPER_OWNER_ACTIVE_ERR="$WORK_DIR/pin_helper_owner_active_view.err"
if pgy_run "$PIN_HELPER_OWNER_ACTIVE_SRC" --backend=c --error-format=json 2>"$PIN_HELPER_OWNER_ACTIVE_ERR"; then
    echo "[diag-json] pin-helper-owner-active-view: FAIL -- expected non-zero exit" >&2
    cat "$PIN_HELPER_OWNER_ACTIVE_ERR" >&2
    exit 1
fi
check_json "pin-helper-owner-active-view" "$PIN_HELPER_OWNER_ACTIVE_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_PARALLEL_CONFLICT" and d.get("cause_ir") == "semantic:pin:parallel_conflict" and d.get("fix_source") == "serialize-pin-access" and "Cannot pass slot" in d.get("message", "") and "while ReadView" in d.get("message", "") for d in data)'

PIN_LIST_OWNER_ACTIVE_SRC="$WORK_DIR/pin_list_owner_active_view.pgy"
cat > "$PIN_LIST_OWNER_ACTIVE_SRC" <<'EOF'
func Main() -> Void {
    let scores: Slot<Int> = ClaimSlot();
    let items: List<Slot<Int>> = ListNew();
    pin scores as view: ReadView<Int> {
        ListPush(items, scores);
    }
}
EOF
PIN_LIST_OWNER_ACTIVE_ERR="$WORK_DIR/pin_list_owner_active_view.err"
if pgy_run "$PIN_LIST_OWNER_ACTIVE_SRC" --backend=c --error-format=json 2>"$PIN_LIST_OWNER_ACTIVE_ERR"; then
    echo "[diag-json] pin-list-owner-active-view: FAIL -- expected non-zero exit" >&2
    cat "$PIN_LIST_OWNER_ACTIVE_ERR" >&2
    exit 1
fi
check_json "pin-list-owner-active-view" "$PIN_LIST_OWNER_ACTIVE_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_PARALLEL_CONFLICT" and d.get("cause_ir") == "semantic:pin:parallel_conflict" and d.get("fix_source") == "serialize-pin-access" and "Cannot store or forward slot" in d.get("message", "") and "through list" in d.get("message", "") and "while ReadView" in d.get("message", "") for d in data)'

PIN_ARRAY_LITERAL_OWNER_ACTIVE_SRC="$WORK_DIR/pin_array_literal_owner_active_view.pgy"
cat > "$PIN_ARRAY_LITERAL_OWNER_ACTIVE_SRC" <<'EOF'
func Main() -> Void {
    let scores: Slot<Int> = ClaimSlot();
    pin scores as view: ReadView<Int> {
        let items: Array<Slot<Int>> = [scores];
    }
}
EOF
PIN_ARRAY_LITERAL_OWNER_ACTIVE_ERR="$WORK_DIR/pin_array_literal_owner_active_view.err"
if pgy_run "$PIN_ARRAY_LITERAL_OWNER_ACTIVE_SRC" --backend=c --error-format=json 2>"$PIN_ARRAY_LITERAL_OWNER_ACTIVE_ERR"; then
    echo "[diag-json] pin-array-literal-owner-active-view: FAIL -- expected non-zero exit" >&2
    cat "$PIN_ARRAY_LITERAL_OWNER_ACTIVE_ERR" >&2
    exit 1
fi
check_json "pin-array-literal-owner-active-view" "$PIN_ARRAY_LITERAL_OWNER_ACTIVE_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_PARALLEL_CONFLICT" and d.get("cause_ir") == "semantic:pin:parallel_conflict" and d.get("fix_source") == "serialize-pin-access" and "Cannot store or forward slot" in d.get("message", "") and "through array literal" in d.get("message", "") and "while ReadView" in d.get("message", "") for d in data)'

PIN_RETURN_OWNER_ACTIVE_SRC="$WORK_DIR/pin_return_owner_active_view.pgy"
cat > "$PIN_RETURN_OWNER_ACTIVE_SRC" <<'EOF'
func Leak() -> Slot<Int> {
    let scores: Slot<Int> = ClaimSlot();
    pin scores as view: ReadView<Int> {
        return scores;
    }
}
func Main() -> Void {
    Log(1);
}
EOF
PIN_RETURN_OWNER_ACTIVE_ERR="$WORK_DIR/pin_return_owner_active_view.err"
if pgy_run "$PIN_RETURN_OWNER_ACTIVE_SRC" --backend=c --error-format=json 2>"$PIN_RETURN_OWNER_ACTIVE_ERR"; then
    echo "[diag-json] pin-return-owner-active-view: FAIL -- expected non-zero exit" >&2
    cat "$PIN_RETURN_OWNER_ACTIVE_ERR" >&2
    exit 1
fi
check_json "pin-return-owner-active-view" "$PIN_RETURN_OWNER_ACTIVE_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_PARALLEL_CONFLICT" and d.get("cause_ir") == "semantic:pin:parallel_conflict" and d.get("fix_source") == "serialize-pin-access" and "Cannot store or forward slot" in d.get("message", "") and "through return" in d.get("message", "") and "while ReadView" in d.get("message", "") for d in data)'

PIN_BOX_OWNER_ACTIVE_SRC="$WORK_DIR/pin_box_owner_active_view.pgy"
cat > "$PIN_BOX_OWNER_ACTIVE_SRC" <<'EOF'
func Main() -> Void {
    let scores: Slot<Int> = ClaimSlot();
    pin scores as view: ReadView<Int> {
        let boxed: Box<Slot<Int>> = Box(scores);
    }
}
EOF
PIN_BOX_OWNER_ACTIVE_ERR="$WORK_DIR/pin_box_owner_active_view.err"
if pgy_run "$PIN_BOX_OWNER_ACTIVE_SRC" --backend=c --error-format=json 2>"$PIN_BOX_OWNER_ACTIVE_ERR"; then
    echo "[diag-json] pin-box-owner-active-view: FAIL -- expected non-zero exit" >&2
    cat "$PIN_BOX_OWNER_ACTIVE_ERR" >&2
    exit 1
fi
check_json "pin-box-owner-active-view" "$PIN_BOX_OWNER_ACTIVE_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_PARALLEL_CONFLICT" and d.get("cause_ir") == "semantic:pin:parallel_conflict" and d.get("fix_source") == "serialize-pin-access" and "Cannot store or forward slot" in d.get("message", "") and "through box" in d.get("message", "") and "while ReadView" in d.get("message", "") for d in data)'

BOX_RESOURCE_HANDLE_SRC="$WORK_DIR/box_resource_handle_payload.pgy"
cat > "$BOX_RESOURCE_HANDLE_SRC" <<'EOF'
func Main() -> Void {
    let scores: Slot<Int> = ClaimSlot();
    let boxed: Box<Slot<Int>> = Box(scores);
}
EOF
BOX_RESOURCE_HANDLE_ERR="$WORK_DIR/box_resource_handle_payload.err"
if pgy_run "$BOX_RESOURCE_HANDLE_SRC" --backend=c --error-format=json 2>"$BOX_RESOURCE_HANDLE_ERR"; then
    echo "[diag-json] box-resource-handle-payload: FAIL -- expected non-zero exit" >&2
    cat "$BOX_RESOURCE_HANDLE_ERR" >&2
    exit 1
fi
check_json "box-resource-handle-payload" "$BOX_RESOURCE_HANDLE_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_BUILTIN_ARGS_INVALID" and d.get("cause_ir") == "semantic:builtin:signature_mismatch" and d.get("fix_source") == "match-builtin-signature" and "Box<T> beta-stable payloads cannot be resource handles" in d.get("message", "") for d in data)'

PIN_SPAWN_SRC="$WORK_DIR/pin_spawn_boundary.pgy"
cat > "$PIN_SPAWN_SRC" <<'EOF'
func Work() -> Int {
    return 1;
}

func Main() -> Void {
    let scores: Slot<Int> = ClaimSlot();
    let view: ReadView<Int> = ViewRead(scores);
    let pending: Future<Int> = spawn Work();
    Log(1);
}
EOF
PIN_SPAWN_ERR="$WORK_DIR/pin_spawn_boundary.err"
if pgy_run "$PIN_SPAWN_SRC" --backend=c --error-format=json 2>"$PIN_SPAWN_ERR"; then
    echo "[diag-json] pin-spawn-boundary: FAIL -- expected non-zero exit" >&2
    cat "$PIN_SPAWN_ERR" >&2
    exit 1
fi
check_json "pin-spawn-boundary" "$PIN_SPAWN_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_AWAIT_BOUNDARY" and d.get("cause_ir") == "semantic:pin:await_boundary" and d.get("fix_source") == "end-pin-before-await" and "spawn suspension boundary" in d.get("message", "") for d in data)'

PIN_ASYNC_SRC="$WORK_DIR/pin_async_boundary.pgy"
cat > "$PIN_ASYNC_SRC" <<'EOF'
func Main() -> Void {
    let scores: Slot<Int> = ClaimSlot();
    let view: ReadView<Int> = ViewRead(scores);
    async { Log(1); }
}
EOF
PIN_ASYNC_ERR="$WORK_DIR/pin_async_boundary.err"
if pgy_run "$PIN_ASYNC_SRC" --backend=c --error-format=json 2>"$PIN_ASYNC_ERR"; then
    echo "[diag-json] pin-async-boundary: FAIL -- expected non-zero exit" >&2
    cat "$PIN_ASYNC_ERR" >&2
    exit 1
fi
check_json "pin-async-boundary" "$PIN_ASYNC_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_AWAIT_BOUNDARY" and d.get("cause_ir") == "semantic:pin:await_boundary" and d.get("fix_source") == "end-pin-before-await" and "async block boundary" in d.get("message", "") for d in data)'

PIN_EVENT_CALLBACK_SRC="$WORK_DIR/pin_event_callback_boundary.pgy"
cat > "$PIN_EVENT_CALLBACK_SRC" <<'EOF'
event OnDamage(amount: Int);

func Main() -> Void {
    let scores: Slot<Int> = ClaimSlot();
    let view: ReadView<Int> = ViewRead(scores);
    OnDamage += (amount: Int) => { Log(amount); };
}
EOF
PIN_EVENT_CALLBACK_ERR="$WORK_DIR/pin_event_callback_boundary.err"
if pgy_run "$PIN_EVENT_CALLBACK_SRC" --backend=c --error-format=json 2>"$PIN_EVENT_CALLBACK_ERR"; then
    echo "[diag-json] pin-event-callback-boundary: FAIL -- expected non-zero exit" >&2
    cat "$PIN_EVENT_CALLBACK_ERR" >&2
    exit 1
fi
check_json "pin-event-callback-boundary" "$PIN_EVENT_CALLBACK_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_AWAIT_BOUNDARY" and d.get("cause_ir") == "semantic:pin:await_boundary" and d.get("fix_source") == "end-pin-before-await" and "event callback boundary" in d.get("message", "") for d in data)'

PIN_CHANNEL_SRC="$WORK_DIR/pin_channel_boundary.pgy"
cat > "$PIN_CHANNEL_SRC" <<'EOF'
func Main() -> Void {
    let scores: Slot<Int> = ClaimSlot();
    let view: ReadView<Int> = ViewRead(scores);
    let ch: Channel<Int> = Channel(1);
    ch <- 1;
    Log(1);
}
EOF
PIN_CHANNEL_ERR="$WORK_DIR/pin_channel_boundary.err"
if pgy_run "$PIN_CHANNEL_SRC" --backend=c --error-format=json 2>"$PIN_CHANNEL_ERR"; then
    echo "[diag-json] pin-channel-boundary: FAIL -- expected non-zero exit" >&2
    cat "$PIN_CHANNEL_ERR" >&2
    exit 1
fi
check_json "pin-channel-boundary" "$PIN_CHANNEL_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_AWAIT_BOUNDARY" and d.get("cause_ir") == "semantic:pin:await_boundary" and d.get("fix_source") == "end-pin-before-await" and "channel handoff boundary" in d.get("message", "") for d in data)'

PIN_CANCEL_SRC="$WORK_DIR/pin_cancel_boundary.pgy"
cat > "$PIN_CANCEL_SRC" <<'EOF'
func Work() -> Int {
    return 1;
}

func Main() -> Void {
    let pending: Future<Int> = spawn Work();
    let scores: Slot<Int> = ClaimSlot();
    let view: ReadView<Int> = ViewRead(scores);
    Cancel(pending);
    Log(1);
}
EOF
PIN_CANCEL_ERR="$WORK_DIR/pin_cancel_boundary.err"
if pgy_run "$PIN_CANCEL_SRC" --backend=c --error-format=json 2>"$PIN_CANCEL_ERR"; then
    echo "[diag-json] pin-cancel-boundary: FAIL -- expected non-zero exit" >&2
    cat "$PIN_CANCEL_ERR" >&2
    exit 1
fi
check_json "pin-cancel-boundary" "$PIN_CANCEL_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_AWAIT_BOUNDARY" and d.get("cause_ir") == "semantic:pin:await_boundary" and d.get("fix_source") == "end-pin-before-await" and "cancel cleanup boundary" in d.get("message", "") for d in data)'

PIN_SOURCE_CANCEL_SRC="$WORK_DIR/pin_source_cancel_boundary.pgy"
cat > "$PIN_SOURCE_CANCEL_SRC" <<'EOF'
func Work() -> Int {
    return 1;
}

func Main() -> Void {
    let pending: Future<Int> = spawn Work();
    let scores: Slot<Int> = ClaimSlot();
    pin scores as view: ReadView<Int> {
        Cancel(pending);
        Log(1);
    }
}
EOF
PIN_SOURCE_CANCEL_ERR="$WORK_DIR/pin_source_cancel_boundary.err"
if pgy_run "$PIN_SOURCE_CANCEL_SRC" --backend=c --error-format=json 2>"$PIN_SOURCE_CANCEL_ERR"; then
    echo "[diag-json] pin-source-cancel-boundary: FAIL -- expected non-zero exit" >&2
    cat "$PIN_SOURCE_CANCEL_ERR" >&2
    exit 1
fi
check_json "pin-source-cancel-boundary" "$PIN_SOURCE_CANCEL_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_AWAIT_BOUNDARY" and d.get("cause_ir") == "semantic:pin:await_boundary" and d.get("fix_source") == "end-pin-before-await" and "cancel cleanup boundary" in d.get("message", "") for d in data)'

PIN_DEFER_SRC="$WORK_DIR/pin_defer_boundary.pgy"
cat > "$PIN_DEFER_SRC" <<'EOF'
func Main() -> Void {
    let scores: Slot<Int> = ClaimSlot();
    let view: ReadView<Int> = ViewRead(scores);
    defer {
        let value: Int = Read(view);
        Log(value);
    };
    Log(1);
}
EOF
PIN_DEFER_ERR="$WORK_DIR/pin_defer_boundary.err"
if pgy_run "$PIN_DEFER_SRC" --backend=c --error-format=json 2>"$PIN_DEFER_ERR"; then
    echo "[diag-json] pin-defer-boundary: FAIL -- expected non-zero exit" >&2
    cat "$PIN_DEFER_ERR" >&2
    exit 1
fi
check_json "pin-defer-boundary" "$PIN_DEFER_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_AWAIT_BOUNDARY" and d.get("cause_ir") == "semantic:pin:await_boundary" and d.get("fix_source") == "end-pin-before-await" and "defer cleanup boundary" in d.get("message", "") for d in data)'

PIN_SOURCE_DEFER_SRC="$WORK_DIR/pin_source_defer_boundary.pgy"
cat > "$PIN_SOURCE_DEFER_SRC" <<'EOF'
func Main() -> Void {
    let scores: Slot<Int> = ClaimSlot();
    Write(scores, 7);
    pin scores as view: ReadView<Int> {
        defer {
            let value: Int = Read(view);
            Log(value);
        };
    }
    Log(1);
}
EOF
PIN_SOURCE_DEFER_ERR="$WORK_DIR/pin_source_defer_boundary.err"
if pgy_run "$PIN_SOURCE_DEFER_SRC" --backend=c --error-format=json 2>"$PIN_SOURCE_DEFER_ERR"; then
    echo "[diag-json] pin-source-defer-boundary: FAIL -- expected non-zero exit" >&2
    cat "$PIN_SOURCE_DEFER_ERR" >&2
    exit 1
fi
check_json "pin-source-defer-boundary" "$PIN_SOURCE_DEFER_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_AWAIT_BOUNDARY" and d.get("cause_ir") == "semantic:pin:await_boundary" and d.get("fix_source") == "end-pin-before-await" and "defer cleanup boundary" in d.get("message", "") for d in data)'

DYNAMIC_DEFER_SRC="$WORK_DIR/dynamic_defer_control.pgy"
cat > "$DYNAMIC_DEFER_SRC" <<'EOF'
func Main(flag: Bool) -> Void {
    if flag {
        defer {
            Log(1);
        };
    }
    Log(2);
}
EOF
DYNAMIC_DEFER_ERR="$WORK_DIR/dynamic_defer_control.err"
if pgy_run "$DYNAMIC_DEFER_SRC" --backend=c --error-format=json 2>"$DYNAMIC_DEFER_ERR"; then
    echo "[diag-json] dynamic-defer-control: FAIL -- expected non-zero exit" >&2
    cat "$DYNAMIC_DEFER_ERR" >&2
    exit 1
fi
check_json "dynamic-defer-control" "$DYNAMIC_DEFER_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_DEFER_DYNAMIC_CONTROL" and d.get("cause_ir") == "semantic:defer:dynamic_control" and d.get("fix_source") == "move-defer-outside-dynamic-control" for d in data)'

PIN_AWAIT_SRC="$WORK_DIR/pin_await_boundary.pgy"
cat > "$PIN_AWAIT_SRC" <<'EOF'
async func Main() -> Void {
    let scores: Slot<Int> = ClaimSlot();
    let view: ReadView<Int> = ViewRead(scores);
    let pending = spawn 42;
    let value = await pending;
    Log(value);
}
EOF
PIN_AWAIT_ERR="$WORK_DIR/pin_await_boundary.err"
if pgy_run "$PIN_AWAIT_SRC" --backend=c --error-format=json 2>"$PIN_AWAIT_ERR"; then
    echo "[diag-json] pin-await-boundary: FAIL -- expected non-zero exit" >&2
    cat "$PIN_AWAIT_ERR" >&2
    exit 1
fi
check_json "pin-await-boundary" "$PIN_AWAIT_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_AWAIT_BOUNDARY" and d.get("cause_ir") == "semantic:pin:await_boundary" and d.get("fix_source") == "end-pin-before-await" for d in data)'

PIN_QUBIT_SRC="$WORK_DIR/pin_qubit_reject.pgy"
cat > "$PIN_QUBIT_SRC" <<'EOF'
func Main() -> Void {
    let q: QubitSlot = ClaimQubit();
    let view: ReadView<QubitSlot> = ViewRead(q);
}
EOF
PIN_QUBIT_ERR="$WORK_DIR/pin_qubit_reject.err"
if pgy_run "$PIN_QUBIT_SRC" --backend=c --error-format=json 2>"$PIN_QUBIT_ERR"; then
    echo "[diag-json] pin-qubit-reject: FAIL -- expected non-zero exit" >&2
    cat "$PIN_QUBIT_ERR" >&2
    exit 1
fi
check_json "pin-qubit-reject" "$PIN_QUBIT_ERR" \
  'isinstance(data, list) and any(d.get("stage") == "semantic" and d.get("code") == "PGY_SEM_PIN_QUBIT_REJECT" and d.get("cause_ir") == "semantic:pin:qubit_reject" and d.get("fix_source") == "do-not-pin-qubit" for d in data)'

# --- case 2a: lex error ---
LEX_SRC="$WORK_DIR/lex.pgy"
cat > "$LEX_SRC" <<'EOF'
func Main() -> Void {
    ~
}
EOF
LEX_ERR="$WORK_DIR/lex.err"
if pgy_run "$LEX_SRC" --backend=c --error-format=json 2>"$LEX_ERR"; then
    echo "[diag-json] lex: FAIL: expected non-zero exit" >&2
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
if pgy_run "$AIR_SRC" --backend=c --error-format=json 2>"$AIR_ERR"; then
    echo "[diag-json] air: FAIL -- expected non-zero exit" >&2
    cat "$AIR_ERR" >&2
    exit 1
fi
check_json "air-evidence" "$AIR_ERR" \
  'isinstance(data, list) and len(data) == 1 and data[0].get("stage") == "semantic" and data[0].get("code") == "PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING" and data[0].get("location", {}).get("line", 0) > 0 and data[0].get("location", {}).get("column", 0) > 0'
check_json "air-evidence-hints" "$AIR_ERR" \
  'isinstance(data, list) and data[0].get("cause_ir") == "semantic:intent:boundary_evidence" and data[0].get("fix_source") == "align-intent-boundary-evidence" and "expected authority participant(s): buyer" in data[0].get("message", "") and "evidence hir=" in data[0].get("message", "") and "hir_cfg=" in data[0].get("message", "") and "rir_boundary=PaymentZone" in data[0].get("message", "") and "rir_authority=<none>" in data[0].get("message", "")'

# --- case 2c: AIR strict evidence from parsed IO boundary ---
AIR_IO_SRC="$WORK_DIR/air_io_boundary_evidence.pgy"
cat > "$AIR_IO_SRC" <<'EOF'
subject Loader { let hp: Int; }
zone LoadZone {
    subject slot loader: Loader
}
intent Load(load: LoadZone, loader: Loader) {
    step read {
        where: LoadZone;
        using: load;
        who: loader;
        on: ReadFile("missing.txt");
        expect: true;
    }
    success: true;
    failure: false;
}
func Main() -> Void { }
EOF
AIR_IO_ERR="$WORK_DIR/air_io_boundary_evidence.err"
AIR_IO_OUT="$WORK_DIR/air_io_boundary_evidence.out"
if ! pgy_run "$AIR_IO_SRC" --backend=c --error-format=json >"$AIR_IO_OUT" 2>"$AIR_IO_ERR"; then
    echo "[diag-json] air-io: FAIL -- expected exact IO evidence success" >&2
    cat "$AIR_IO_ERR" >&2
    exit 1
fi
check_json "air-io-evidence" "$AIR_IO_ERR" \
  'isinstance(data, list) and len(data) == 0'

# --- case 2d: intent control-transfer explicit reject keeps source span ---
INTENT_CONTROL_SRC="$WORK_DIR/intent_control_transfer_reject.pgy"
cat > "$INTENT_CONTROL_SRC" <<'EOF'
subject Worker { let hp: Int; }
zone WorkZone {
    subject slot worker: Worker
}
func DoWork() -> Int { return 1; }
intent Dispatch(work: WorkZone, worker: Worker) {
    step run {
        where: WorkZone;
        using: work;
        who: worker;
        on: spawn DoWork();
        expect: true;
    }
    success: true;
    failure: false;
}
func Main() -> Void { }
EOF
INTENT_CONTROL_ERR="$WORK_DIR/intent_control_transfer_reject.err"
if pgy_run "$INTENT_CONTROL_SRC" --backend=c --error-format=json 2>"$INTENT_CONTROL_ERR"; then
    echo "[diag-json] intent-control-transfer: FAIL -- expected non-zero exit" >&2
    cat "$INTENT_CONTROL_ERR" >&2
    exit 1
fi
check_json "intent-control-transfer" "$INTENT_CONTROL_ERR" \
  'isinstance(data, list) and len(data) == 1 and data[0].get("stage") == "semantic" and data[0].get("code") == "PGY_SEM_INTENT_STEP_INVALID" and data[0].get("location", {}).get("line", 0) > 0 and data[0].get("location", {}).get("column", 0) > 0'
check_json "intent-control-transfer-hints" "$INTENT_CONTROL_ERR" \
  'isinstance(data, list) and data[0].get("cause_ir") == "semantic:intent_step" and data[0].get("fix_source") == "align-step-with-zone-action-contracts" and "cannot contain" in data[0].get("message", "") and "spawn" in data[0].get("message", "")'

# --- case 2e: intent channel control-transfer explicit reject keeps source span ---
INTENT_CHANNEL_SRC="$WORK_DIR/intent_channel_transfer_reject.pgy"
cat > "$INTENT_CHANNEL_SRC" <<'EOF'
subject Worker { let hp: Int; }
zone WorkZone {
    subject slot worker: Worker
}
intent Dispatch(work: WorkZone, worker: Worker, ch: Channel<Int>) {
    step run {
        where: WorkZone;
        using: work;
        who: worker;
        on: ch <- 1;
        expect: true;
    }
    success: true;
    failure: false;
}
func Main() -> Void { }
EOF
INTENT_CHANNEL_ERR="$WORK_DIR/intent_channel_transfer_reject.err"
if pgy_run "$INTENT_CHANNEL_SRC" --backend=c --error-format=json 2>"$INTENT_CHANNEL_ERR"; then
    echo "[diag-json] intent-channel-transfer: FAIL -- expected non-zero exit" >&2
    cat "$INTENT_CHANNEL_ERR" >&2
    exit 1
fi
check_json "intent-channel-transfer" "$INTENT_CHANNEL_ERR" \
  'isinstance(data, list) and len(data) == 1 and data[0].get("stage") == "semantic" and data[0].get("code") == "PGY_SEM_INTENT_STEP_INVALID" and data[0].get("location", {}).get("line", 0) > 0 and data[0].get("location", {}).get("column", 0) > 0'
check_json "intent-channel-transfer-hints" "$INTENT_CHANNEL_ERR" \
  'isinstance(data, list) and data[0].get("cause_ir") == "semantic:intent_step" and data[0].get("fix_source") == "align-step-with-zone-action-contracts" and "cannot contain" in data[0].get("message", "") and "channel send" in data[0].get("message", "")'

# --- case 2f: slot lifecycle code routes through ---
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
if pgy_run "$SLOT_SRC" --backend=c --error-format=json 2>"$SLOT_ERR"; then
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
if pgy_run "$OWN_SRC" --backend=c --error-format=json 2>"$OWN_ERR"; then
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
if pgy_run "$SPEC_SRC" --backend=llvm --error-format=json -o "$SPEC_OBJ" \
        >/dev/null 2>"$SPEC_ERR"; then
    # If the LLVM backend isn't wired or the spec limit was raised, skip.
    echo "[diag-json] spec-limit: SKIP (backend did not exercise MAX_LLVM_RESULT_SPECS)"
elif grep -Fq "compiled without LLVM backend support" "$SPEC_ERR"; then
    echo "[diag-json] spec-limit: SKIP (compiler built without LLVM backend support)"
else
    check_json "spec-limit" "$SPEC_ERR" \
      'isinstance(data, list) and any(d.get("code") == "PGY_LLVM_SPEC_LIMIT" and d.get("stage") == "llvm_codegen" for d in data)'
    # Backend hints propagate through LLVMGenCtx → LLVMGenResult →
    # CompilerResult → driver_emit_single_diag_json_full.
    check_json "spec-limit-hints" "$SPEC_ERR" \
      'isinstance(data, list) and any(d.get("cause_ir") == "llvm:result_spec:capacity_exceeded" and d.get("fix_source") == "reuse-shared-error-enum" for d in data)'
fi

# --- case 2c.1: LLVM channel runtime lookup has no silent fallback ---
# Channel<Bool> is allowed through the frontend here, but the beta LLVM
# runtime only declares concrete channel helpers for Int/String. LLVM codegen
# must reject the missing runtime helper with a structured diagnostic instead
# of silently lowering to i32/false/None.
LLVM_CHANNEL_SRC="$WORK_DIR/llvm_channel_runtime_missing.pgy"
cat > "$LLVM_CHANNEL_SRC" <<'EOF'
func Main() -> Void {
    let ch: Channel<Bool> = Channel(1);
    let got: Option<Bool> = TryRecv(ch);
}
EOF
LLVM_CHANNEL_ERR="$WORK_DIR/llvm_channel_runtime_missing.err"
LLVM_CHANNEL_OBJ="$WORK_DIR/llvm_channel_runtime_missing_obj"
if pgy_run "$LLVM_CHANNEL_SRC" --backend=llvm --error-format=json -o "$LLVM_CHANNEL_OBJ" \
        >/dev/null 2>"$LLVM_CHANNEL_ERR"; then
    echo "[diag-json] llvm-channel-runtime-missing: FAIL -- expected missing runtime helper" >&2
    exit 1
elif grep -Fq "compiled without LLVM backend support" "$LLVM_CHANNEL_ERR"; then
    echo "[diag-json] llvm-channel-runtime-missing: SKIP (compiler built without LLVM backend support)"
else
    check_json "llvm-channel-runtime-missing" "$LLVM_CHANNEL_ERR" \
      'isinstance(data, list) and any(d.get("stage") == "llvm_codegen" and d.get("code") == "PGY_LLVM_TYPE_UNSUPPORTED" and d.get("cause_ir") == "llvm:type:unsupported_or_unknown" and d.get("fix_source") == "inspect-mir-inventory" and ("pgy_channel_init_Bool" in d.get("message", "") or "pgy_channel_try_recv_Bool" in d.get("message", "")) for d in data)'
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
if ! pgy_run "$OK_SRC" --backend=c --error-format=json -o "$OK_BIN" \
        >"$OK_OUT" 2>"$OK_ERR"; then
    echo "[diag-json] ok: FAIL — expected zero exit" >&2
    cat "$OK_ERR" >&2
    exit 1
fi
check_json "ok-empty" "$OK_ERR" 'isinstance(data, list) and len(data) == 0'

echo "[diag-json] all cases pass"

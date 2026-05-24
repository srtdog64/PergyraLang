#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY_BIN_WAS_DEFAULT=0
if [[ -z "${PGY_BIN:-}" ]]; then
    PGY_BIN="$ROOT_DIR/bin/pgy"
    PGY_BIN_WAS_DEFAULT=1
fi
if [[ "$PGY_BIN" != *.exe && -x "${PGY_BIN}.exe" ]]; then
    PGY_BIN="${PGY_BIN}.exe"
fi
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pgy-raw-escape.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

require_term() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" ||
        { echo "[raw-escape-contract] $rel missing term: $term" >&2; exit 1; }
}

require_term "src/semantic/type_checker_builtins_nominal.c" "PGY_CODE_SEM_RAW_ESCAPE_UNSTABLE"
require_term "src/semantic/type_checker_builtins_nominal.c" "PGY_CAUSE_RAW_ESCAPE_UNSTABLE"
require_term "src/semantic/type_checker_builtins_nominal.c" "PGY_FIX_USE_PIN_OR_WAIT_FOR_RAW_ESCAPE_CONTRACT"
require_term "src/semantic/type_checker_builtins_nominal.c" "unsafe { } is only a lexical escape marker"
require_term "src/semantic/type_checker_builtins_nominal.c" "scoped unsafe(raw) capability"
require_term "src/semantic/type_checker_builtins_resolve.c" '{"SlotRawPointer", BUILTIN_SLOT_RAW_POINTER}'
require_term "src/semantic/diag_codes.h" "PGY_SEM_RAW_ESCAPE_UNSTABLE"
require_term "src/semantic/diag_codes.h" "semantic:raw_escape:unstable"
require_term "src/parser/parser_stmt.c" "Scoped unsafe capability syntax"
require_term "src/parser/parser_stmt.c" "Scoped unsafe capability label syntax"
require_term "src/parser/parser_stmt.c" "not a universal mode bit"
require_term "src/tests/parser/test_parser_special_part_c.cases.h" "run_reserved_scoped_unsafe_diagnostic_test"
require_term "src/tests/parser/test_parser_special_part_c.cases.h" "run_reserved_labeled_unsafe_diagnostic_test"
require_term "src/test_parser.c" "run_reserved_scoped_unsafe_diagnostic_test"
require_term "src/test_parser.c" "run_reserved_labeled_unsafe_diagnostic_test"
require_term "docs/132_unsafe_capability_scope.md" "Unsafe is a scoped capability, not a mode bit."
require_term "docs/132_unsafe_capability_scope.md" "unsafe(raw)"
require_term "docs/132_unsafe_capability_scope.md" "universal escape hatch"
require_term "docs/100_beta_readiness_checklist.md" "Freeze unsafe as scoped capability, not a mode bit."
require_term "docs/grammar/01_syntax.md" "Raw escape requires a future scoped capability contract"
require_term "docs/19_design_philosophy.md" "must be scoped capability based"
require_term "docs/19_design_philosophy.md" "not become a universal mode bit"
require_term "docs/self_hosted/01_staged_roadmap.md" "scoped unsafe raw-escape contracts"
require_term "docs/self_hosted/01_staged_roadmap.md" "must not grant raw/system-tier escape"
require_term "TODO.md" "Implementation order:"
require_term "TODO.md" "parser-reserved with an explicit diagnostic"
require_term "TODO.md" "Add AST storage for an unsafe capability list"
require_term "TODO.md" "Add semantic scope tracking"
require_term "TODO.md" "Publish unsafe capability evidence into AIR"
require_term "TODO.md" "Add C/LLVM parity only after AIR evidence exists"
require_term "TODO.md" "grant raw pointer access is a beta-blocking regression"

if [[ ! -x "$PGY_BIN" ]]; then
    if [[ "$PGY_BIN_WAS_DEFAULT" -eq 1 ]]; then
        echo "[raw-escape-contract] SKIP executable probe; source contract is gated"
        exit 0
    fi
    echo "[raw-escape-contract] missing compiler binary: $PGY_BIN" >&2
    exit 1
fi
if ! "$PGY_BIN" --help >"$WORK_DIR/pgy-help.out" 2>"$WORK_DIR/pgy-help.err"; then
    if [[ "$PGY_BIN_WAS_DEFAULT" -eq 1 ]]; then
        echo "[raw-escape-contract] SKIP executable probe; source contract is gated"
        exit 0
    fi
    case "$(uname -s 2>/dev/null || true)" in
        MINGW*|MSYS*|CYGWIN*)
            echo "[raw-escape-contract] SKIP executable probe under Windows bash; source contract is gated"
            exit 0
            ;;
    esac
    echo "[raw-escape-contract] compiler binary is not runnable: $PGY_BIN" >&2
    cat "$WORK_DIR/pgy-help.err" >&2
    exit 1
fi

cat >"$WORK_DIR/raw_escape.pgy" <<'PGY'
func Main() -> Void {
    let slot: Slot<Int> = ClaimSlot<Int>();
    unsafe {
        let raw = SlotRawPointer(slot);
        Log(1);
    }
    Release(slot);
}
PGY

if "$PGY_BIN" "$WORK_DIR/raw_escape.pgy" --error-format=json >"$WORK_DIR/out.txt" 2>"$WORK_DIR/err.json"; then
    echo "[raw-escape-contract] expected SlotRawPointer rejection" >&2
    exit 1
fi

for term in \
    "PGY_SEM_RAW_ESCAPE_UNSTABLE" \
    "\"stage\":\"semantic\"" \
    "semantic:raw_escape:unstable" \
    "use-pin-or-wait-for-raw-escape-contract" \
    "unsafe { } is only a lexical escape marker" \
    "scoped unsafe(raw) capability" \
    "typed Pin/Lease views"; do
    if ! grep -Fq "$term" "$WORK_DIR/err.json"; then
        echo "[raw-escape-contract] missing diagnostic term: $term" >&2
        cat "$WORK_DIR/err.json" >&2
        exit 1
    fi
done

echo "[raw-escape-contract] system-tier raw escape is explicitly rejected"

#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON_BIN="${PYTHON_BIN:-}"
CONTRACT_CHECK_DONE=0

require_literal() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" || {
        echo "[runtime-authority-contract] $rel missing term: $term" >&2
        exit 1
    }
}

forbid_literal() {
    local rel="$1"
    local term="$2"
    if grep -Fq -- "$term" "$ROOT_DIR/$rel"; then
        echo "[runtime-authority-contract] $rel contains raw authority literal: $term" >&2
        exit 1
    fi
}

run_literal_contract_smoke() {
    local required_files=(
        "src/runtime/pgy_runtime_authority_contract.h"
        "src/runtime/pgy_runtime_zone_result_option_inline.h"
        "src/runtime/pgy_runtime_lib_authority_file_core.h"
        "src/runtime/pgy_runtime_platform_io_core.h"
        "src/codegen/llvm_runtime.c"
    )
    local required_macros=(
        "PGY_ZONE_AUTHORITY_CODE_OK"
        "PGY_ZONE_AUTHORITY_CODE_UNKNOWN"
        "PGY_ZONE_AUTHORITY_CODE_MISSING_ZONE"
        "PGY_ZONE_AUTHORITY_CODE_MISSING_PARTICIPANT"
        "PGY_ZONE_AUTHORITY_CODE_TOKEN_MISMATCH"
        "PGY_ZONE_AUTHORITY_REASON_MISSING_ZONE"
        "PGY_ZONE_AUTHORITY_REASON_MISSING_PARTICIPANT"
        "PGY_ZONE_AUTHORITY_REASON_TOKEN_MISMATCH"
        "PGY_ZONE_AUTHORITY_STDERR_MISSING_ZONE"
        "PGY_ZONE_AUTHORITY_STDERR_MISSING_PARTICIPANT"
        "PGY_ZONE_AUTHORITY_STDERR_TOKEN_MISMATCH"
    )

    for rel in "${required_files[@]}"; do
        [[ -f "$ROOT_DIR/$rel" ]] || {
            echo "[runtime-authority-contract] missing contract file: $rel" >&2
            exit 1
        }
    done

    for macro in "${required_macros[@]}"; do
        require_literal "src/runtime/pgy_runtime_authority_contract.h" "$macro"
        require_literal "src/runtime/pgy_runtime_zone_result_option_inline.h" "$macro"
        require_literal "src/runtime/pgy_runtime_lib_authority_file_core.h" "$macro"
    done

    require_literal "src/runtime/pgy_runtime_authority_contract.h" "missing-zone"
    require_literal "src/runtime/pgy_runtime_authority_contract.h" "missing-participant"
    require_literal "src/runtime/pgy_runtime_authority_contract.h" "authority-token-mismatch"
    require_literal "src/runtime/pgy_runtime_lib_authority_file_core.h" "pgy_runtime_authority_contract.h"
    require_literal "src/runtime/pgy_runtime_platform_io_core.h" "pgy_runtime_authority_contract.h"
    require_literal "src/codegen/llvm_runtime.c" "pgy_zone_authority_check_token_export"
    require_literal "src/codegen/llvm_runtime.c" "pgy_zone_authority_validate_token_flags_export"

    forbid_literal "src/runtime/pgy_runtime_zone_result_option_inline.h" "\"missing-zone\""
    forbid_literal "src/runtime/pgy_runtime_zone_result_option_inline.h" "\"missing-participant\""
    forbid_literal "src/runtime/pgy_runtime_zone_result_option_inline.h" "\"authority-token-mismatch\""
    forbid_literal "src/runtime/pgy_runtime_lib_authority_file_core.h" "\"missing-zone\""
    forbid_literal "src/runtime/pgy_runtime_lib_authority_file_core.h" "\"missing-participant\""
    forbid_literal "src/runtime/pgy_runtime_lib_authority_file_core.h" "\"authority-token-mismatch\""

    echo "[runtime-authority-contract] authority failure surface is contract-backed (literal fallback)"
}

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        run_literal_contract_smoke
        CONTRACT_CHECK_DONE=1
    fi
fi

if [[ "$CONTRACT_CHECK_DONE" -eq 0 ]]; then
"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])
header = root / "src" / "runtime" / "pgy_runtime_authority_contract.h"
inline_part = root / "src" / "runtime" / "pgy_runtime_zone_result_option_inline.h"
lib_part = root / "src" / "runtime" / "pgy_runtime_lib_authority_file_core.h"
top_part = root / "src" / "runtime" / "pgy_runtime_platform_io_core.h"

required_macros = {
    "PGY_ZONE_AUTHORITY_CODE_OK": "ok",
    "PGY_ZONE_AUTHORITY_CODE_UNKNOWN": "unknown",
    "PGY_ZONE_AUTHORITY_CODE_MISSING_ZONE": "missing-zone",
    "PGY_ZONE_AUTHORITY_CODE_MISSING_PARTICIPANT": "missing-participant",
    "PGY_ZONE_AUTHORITY_CODE_TOKEN_MISMATCH": "authority-token-mismatch",
    "PGY_ZONE_AUTHORITY_REASON_MISSING_ZONE": "zone authority validation failed: null zone self",
    "PGY_ZONE_AUTHORITY_REASON_MISSING_PARTICIPANT": "zone authority validation failed: null authority participant",
    "PGY_ZONE_AUTHORITY_REASON_TOKEN_MISMATCH": "zone authority validation failed: authority token mismatch",
    "PGY_ZONE_AUTHORITY_STDERR_MISSING_ZONE": "[pgy][authority] zone '%s' entered with null self while validating '%s'\\n",
    "PGY_ZONE_AUTHORITY_STDERR_MISSING_PARTICIPANT": "[pgy][authority] zone '%s' has null authority participant '%s'\\n",
    "PGY_ZONE_AUTHORITY_STDERR_TOKEN_MISMATCH": "[pgy][authority] zone '%s' rejected authority token for '%s'\\n",
}

if not header.exists():
    raise SystemExit("missing runtime authority contract header")

header_text = header.read_text(encoding="utf-8")
for macro, literal in required_macros.items():
    if macro not in header_text:
        raise SystemExit(f"runtime authority contract missing macro {macro}")
    if literal not in header_text:
        raise SystemExit(f"runtime authority contract missing literal for {macro}: {literal}")

implementation_files = [lib_part, top_part]
for path in implementation_files:
    text = path.read_text(encoding="utf-8")
    if "pgy_runtime_authority_contract.h" not in text:
        raise SystemExit(f"{path.relative_to(root)} does not include authority contract header")

for path in [inline_part, lib_part]:
    text = path.read_text(encoding="utf-8")
    required_uses = [
        "PGY_ZONE_AUTHORITY_CODE_OK",
        "PGY_ZONE_AUTHORITY_CODE_UNKNOWN",
        "PGY_ZONE_AUTHORITY_CODE_MISSING_ZONE",
        "PGY_ZONE_AUTHORITY_CODE_MISSING_PARTICIPANT",
        "PGY_ZONE_AUTHORITY_CODE_TOKEN_MISMATCH",
        "PGY_ZONE_AUTHORITY_REASON_MISSING_ZONE",
        "PGY_ZONE_AUTHORITY_REASON_MISSING_PARTICIPANT",
        "PGY_ZONE_AUTHORITY_REASON_TOKEN_MISMATCH",
        "PGY_ZONE_AUTHORITY_STDERR_MISSING_ZONE",
        "PGY_ZONE_AUTHORITY_STDERR_MISSING_PARTICIPANT",
        "PGY_ZONE_AUTHORITY_STDERR_TOKEN_MISMATCH",
    ]
    missing = [macro for macro in required_uses if macro not in text]
    if missing:
        raise SystemExit(
            f"{path.relative_to(root)} missing macro use(s): " + ", ".join(missing)
        )

    raw_literals = [
        '"missing-zone"',
        '"missing-participant"',
        '"authority-token-mismatch"',
        '"zone authority validation failed: null zone self"',
        '"zone authority validation failed: null authority participant"',
        '"zone authority validation failed: authority token mismatch"',
        '"[pgy][authority] zone',
    ]
    offenders = [literal for literal in raw_literals if literal in text]
    if offenders:
        raise SystemExit(
            f"{path.relative_to(root)} contains raw authority literal(s): "
            + ", ".join(offenders)
        )

top_text = top_part.read_text(encoding="utf-8")
if re.search(r'pgy_zone_authority_last_code\[[^\]]+\]\s*=\s*"ok"', top_text):
    raise SystemExit("inline runtime initializes authority code with raw \"ok\"")
if "PGY_ZONE_AUTHORITY_CODE_OK" not in top_text:
    raise SystemExit("inline runtime top part does not use PGY_ZONE_AUTHORITY_CODE_OK")

runtime_decl = root / "src" / "codegen" / "llvm_runtime.c"
runtime_text = runtime_decl.read_text(encoding="utf-8")
required_exports = [
    "pgy_zone_authority_check_token_export",
    "pgy_zone_authority_validate_token_flags_export",
]
for export in required_exports:
    if export not in runtime_text:
        raise SystemExit(f"LLVM runtime registry missing authority token export {export}")

print("[runtime-authority-contract] authority failure surface is contract-backed")
PY
fi

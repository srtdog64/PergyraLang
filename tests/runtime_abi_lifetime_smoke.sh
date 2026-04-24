#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON_BIN="${PYTHON_BIN:-}"

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        echo "missing python for runtime ABI lifetime smoke" >&2
        exit 1
    fi
fi

"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])

groups = {
    "inline-intent": (
        root / "src" / "runtime" / "pgy_runtime_part_ba_part_a.inc",
        [
            "pgy_intent_last_trace_export",
            "pgy_intent_last_failure_export",
            "pgy_intent_last_name_export",
            "pgy_intent_history_step_name_export",
            "pgy_intent_history_step_zone_export",
            "pgy_intent_history_step_phase_export",
            "pgy_intent_history_step_participant_export",
            "pgy_intent_history_step_slot_export",
            "pgy_intent_history_step_from_zone_export",
            "pgy_intent_history_step_from_slot_export",
            "pgy_intent_history_step_to_zone_export",
            "pgy_intent_history_step_to_slot_export",
            "pgy_intent_history_step_failure_export",
        ],
    ),
    "llvm-intent": (
        root / "src" / "runtime" / "pgy_runtime_lib_part_b_part_c.inc",
        [
            "pgy_intent_last_trace_export",
            "pgy_intent_last_failure_export",
            "pgy_intent_last_name_export",
            "pgy_intent_history_step_name_export",
            "pgy_intent_history_step_zone_export",
            "pgy_intent_history_step_phase_export",
            "pgy_intent_history_step_participant_export",
            "pgy_intent_history_step_slot_export",
            "pgy_intent_history_step_from_zone_export",
            "pgy_intent_history_step_from_slot_export",
            "pgy_intent_history_step_to_zone_export",
            "pgy_intent_history_step_to_slot_export",
            "pgy_intent_history_step_failure_export",
            "pgy_intent_active_name_export",
            "pgy_intent_active_trace_export",
            "pgy_intent_active_failure_export",
            "pgy_intent_active_step_name_export",
            "pgy_intent_recent_name_export",
            "pgy_intent_recent_trace_export",
            "pgy_intent_recent_failure_export",
        ],
    ),
    "inline-authority": (
        root / "src" / "runtime" / "pgy_runtime_part_ba_part_f.inc",
        [
            "pgy_zone_authority_last_zone_export",
            "pgy_zone_authority_last_participant_export",
            "pgy_zone_authority_last_code_export",
            "pgy_zone_authority_last_reason_export",
            "pgy_zone_authority_last_zone_rt_export",
            "pgy_zone_authority_last_participant_rt_export",
            "pgy_zone_authority_last_code_rt_export",
            "pgy_zone_authority_last_reason_rt_export",
        ],
    ),
    "llvm-authority": (
        root / "src" / "runtime" / "pgy_runtime_lib_part_a.inc",
        [
            "pgy_zone_authority_last_zone_rt_export",
            "pgy_zone_authority_last_participant_rt_export",
            "pgy_zone_authority_last_code_rt_export",
            "pgy_zone_authority_last_reason_rt_export",
        ],
    ),
}

forbidden = [
    "malloc",
    "calloc",
    "realloc",
    "free",
    "strdup",
    "pgy_runtime_strdup",
    "pgy_runtime_lib_strdup",
    "pgy_intent_copy_string",
]


def find_function_body(text: str, name: str) -> str:
    match = re.search(r"\b" + re.escape(name) + r"\s*\([^)]*\)\s*\{", text)
    if match is None:
        raise SystemExit(f"missing runtime ABI string export function {name}")
    open_index = text.find("{", match.start())
    depth = 0
    in_string = False
    in_char = False
    escaped = False
    for index in range(open_index, len(text)):
        ch = text[index]
        if in_string:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == '"':
                in_string = False
            continue
        if in_char:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == "'":
                in_char = False
            continue
        if ch == '"':
            in_string = True
            continue
        if ch == "'":
            in_char = True
            continue
        if ch == "{":
            depth += 1
            continue
        if ch == "}":
            depth -= 1
            if depth == 0:
                return text[open_index:index + 1]
    raise SystemExit(f"unterminated function body for {name}")


for group_name, (path, functions) in groups.items():
    if not path.exists():
        raise SystemExit(f"missing runtime ABI lifetime source: {path.relative_to(root)}")
    text = path.read_text(encoding="utf-8")
    for fn in functions:
        body = find_function_body(text, fn)
        offenders = [
            token for token in forbidden
            if re.search(r"\b" + re.escape(token) + r"\b", body)
        ]
        if offenders:
            raise SystemExit(
                f"{path.relative_to(root)}:{fn} performs ownership-changing work: "
                + ", ".join(offenders)
            )
        if "return" not in body:
            raise SystemExit(f"{path.relative_to(root)}:{fn} has no return statement")

proof_doc = root / "docs" / "semantics" / "04_ownership_abi.md"
proof_text = proof_doc.read_text(encoding="utf-8")
required_doc_terms = [
    "runtime-borrowed string",
    "caller must not free",
    "valid until the next mutation of the corresponding runtime registry",
    "runtime-abi-lifetime-test-smoke",
]
missing = [term for term in required_doc_terms if term not in proof_text]
if missing:
    raise SystemExit(
        "ownership proof doc missing runtime ABI lifetime term(s): "
        + ", ".join(missing)
    )

print("[runtime-abi-lifetime] runtime string exports are borrowed and allocation-free")
PY


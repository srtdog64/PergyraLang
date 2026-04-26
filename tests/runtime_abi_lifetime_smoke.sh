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
    "inline-intent-active": (
        root / "src" / "runtime" / "pgy_runtime_intent_active_exports.h",
        [
            "pgy_intent_active_name_export",
            "pgy_intent_active_trace_export",
            "pgy_intent_active_failure_export",
            "pgy_intent_active_step_name_export",
        ],
    ),
    "inline-intent-recent": (
        root / "src" / "runtime" / "pgy_runtime_panic_checked_inline.h",
        [
            "pgy_intent_recent_name_export",
            "pgy_intent_recent_trace_export",
            "pgy_intent_recent_failure_export",
        ],
    ),
    "llvm-intent": (
        root / "src" / "runtime" / "pgy_runtime_lib_intent_exports.h",
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
        root / "src" / "runtime" / "pgy_runtime_part_ba_part_e.inc",
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
        root / "src" / "runtime" / "pgy_runtime_lib_authority_file_core.h",
        [
            "pgy_zone_authority_last_zone_rt_export",
            "pgy_zone_authority_last_participant_rt_export",
            "pgy_zone_authority_last_code_rt_export",
            "pgy_zone_authority_last_reason_rt_export",
        ],
    ),
}

macro_exports = {
    "inline-intent-active-step": (
        root / "src" / "runtime" / "pgy_runtime_intent_active_exports.h",
        "PGY_INTENT_ACTIVE_STEP_STRING_EXPORT",
        [
            "pgy_intent_active_step_zone_export",
            "pgy_intent_active_step_phase_export",
            "pgy_intent_active_step_participant_export",
            "pgy_intent_active_step_slot_export",
            "pgy_intent_active_step_from_zone_export",
            "pgy_intent_active_step_from_slot_export",
            "pgy_intent_active_step_to_zone_export",
            "pgy_intent_active_step_to_slot_export",
            "pgy_intent_active_step_failure_export",
        ],
    ),
    "llvm-intent-active-step": (
        root / "src" / "runtime" / "pgy_runtime_lib_intent_exports.h",
        "PGY_INTENT_ACTIVE_STEP_STRING_EXPORT",
        [
            "pgy_intent_active_step_zone_export",
            "pgy_intent_active_step_phase_export",
            "pgy_intent_active_step_participant_export",
            "pgy_intent_active_step_slot_export",
            "pgy_intent_active_step_from_zone_export",
            "pgy_intent_active_step_from_slot_export",
            "pgy_intent_active_step_to_zone_export",
            "pgy_intent_active_step_to_slot_export",
            "pgy_intent_active_step_failure_export",
        ],
    ),
}

result_owned_strings = {
    "inline-string-helpers": (
        root / "src" / "runtime" / "pgy_runtime_part_c.inc",
        [
            "Substring",
            "StringReplace",
            "StringTrim",
            "ToUpper",
            "ToLower",
            "StringConcat",
            "StringJoin",
        ],
        "pgy_runtime_strdup",
    ),
    "llvm-string-helpers": (
        root / "src" / "runtime" / "pgy_runtime_lib_slot_array_io_string_exports.h",
        [
            "pgy_file_read",
            "pgy_read_file",
            "pgy_input",
            "Substring",
            "StringReplace",
            "StringTrim",
            "ToUpper",
            "ToLower",
            "StringConcat",
            "StringJoin",
        ],
        "pgy_runtime_lib_strdup",
    ),
}

result_owned_arrays = {
    "inline-string-array-helpers": (
        root / "src" / "runtime" / "pgy_runtime_part_c.inc",
        [
            "StringSplit",
        ],
        "pgy_runtime_strdup",
    ),
    "llvm-string-array-helpers": (
        root / "src" / "runtime" / "pgy_runtime_lib_slot_array_io_string_exports.h",
        [
            "StringSplit",
            "pgy_map_keys_raw_export",
        ],
        "pgy_runtime_lib_strdup",
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


def read_runtime_text(path: pathlib.Path) -> str:
    text = path.read_text(encoding="utf-8")
    if path.name.startswith("pgy_runtime_part_ba_part_"):
        parts = [
            path.with_name("pgy_runtime_part_ba_part_a.inc"),
            path.with_name("pgy_runtime_intent_active_exports.h"),
            path.with_name("pgy_runtime_intent_history.h"),
            path.with_name("pgy_runtime_intent_exit.h"),
            path.with_name("pgy_runtime_panic_checked_inline.h"),
            path.with_name("pgy_runtime_memory_array_slot_inline.h"),
            path.with_name("pgy_runtime_slot_macros.h"),
            path.with_name("pgy_runtime_part_ba_part_c.inc"),
            path.with_name("pgy_runtime_part_ba_part_d.inc"),
            path.with_name("pgy_runtime_pool_fsm_timer_inline.h"),
            path.with_name("pgy_runtime_part_ba_part_e.inc"),
        ]
        missing = [part for part in parts if not part.exists()]
        if missing:
            raise SystemExit(
                "missing split continuation for runtime ABI helper source: "
                + ", ".join(str(part.relative_to(root)) for part in missing)
            )
        return "\n".join(part.read_text(encoding="utf-8") for part in parts)
    if path.name.startswith("pgy_runtime_lib_part_b_part_"):
        parts = [
            path.with_name("pgy_runtime_lib_core_exports.h"),
            path.with_name("pgy_runtime_lib_raw_collection_exports.h"),
            path.with_name("pgy_runtime_lib_part_b_part_b.inc"),
            path.with_name("pgy_runtime_lib_intent_exports.h"),
            path.with_name("pgy_runtime_lib_part_b_part_c.inc"),
            path.with_name("pgy_runtime_lib_slot_exports.h"),
            path.with_name("pgy_runtime_lib_slot_array_io_string_exports.h"),
            path.with_name("pgy_runtime_lib_std_exports.h"),
            path.with_name("pgy_runtime_lib_channel_quantum_exports.h"),
            path.with_name("pgy_runtime_lib_part_b_part_f.inc"),
        ]
        missing = [part for part in parts if not part.exists()]
        if missing:
            raise SystemExit(
                "missing split continuation for runtime ABI helper source: "
                + ", ".join(str(part.relative_to(root)) for part in missing)
            )
        return "\n".join(part.read_text(encoding="utf-8") for part in parts)
    if path.name == "pgy_runtime_lib_slot_array_io_string_exports.h":
        continuations = [
            path.with_name("pgy_runtime_lib_std_exports.h"),
            path.with_name("pgy_runtime_lib_channel_quantum_exports.h"),
        ]
        missing = [continuation for continuation in continuations if not continuation.exists()]
        if missing:
            raise SystemExit(
                "missing split continuation for runtime ABI helper source: "
                + ", ".join(str(continuation.relative_to(root)) for continuation in missing)
            )
        for continuation in continuations:
            text += "\n" + continuation.read_text(encoding="utf-8")
    return text


for group_name, (path, functions) in groups.items():
    if not path.exists():
        raise SystemExit(f"missing runtime ABI lifetime source: {path.relative_to(root)}")
    text = read_runtime_text(path)
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

result_owned_forbidden_returns = [
    'return ""',
    "return s;",
    "return tmp;",
    "return stack_buf;",
    "return resolved;",
]

for group_name, (path, functions, dup_helper) in result_owned_strings.items():
    if not path.exists():
        raise SystemExit(f"missing runtime ABI result-owned source: {path.relative_to(root)}")
    text = read_runtime_text(path)
    for fn in functions:
        body = find_function_body(text, fn)
        if (dup_helper not in body
                and "pgy_runtime_strdup_export" not in body
                and "malloc" not in body):
            raise SystemExit(
                f"{path.relative_to(root)}:{fn} does not allocate/copy a result-owned string"
            )
        bad_returns = [term for term in result_owned_forbidden_returns if term in body]
        if bad_returns:
            raise SystemExit(
                f"{path.relative_to(root)}:{fn} returns borrowed/stack data in result-owned ABI: "
                + ", ".join(bad_returns)
            )

for group_name, (path, functions, dup_helper) in result_owned_arrays.items():
    if not path.exists():
        raise SystemExit(f"missing runtime ABI result-owned array source: {path.relative_to(root)}")
    text = read_runtime_text(path)
    for fn in functions:
        body = find_function_body(text, fn)
        if "PgyArray_String" not in body and "pgy_array_new_String" not in body:
            raise SystemExit(
                f"{path.relative_to(root)}:{fn} does not materialize a string array payload"
            )
        if (dup_helper not in body
                and "pgy_runtime_strdup_export" not in body
                and "malloc" not in body):
            raise SystemExit(
                f"{path.relative_to(root)}:{fn} does not allocate/copy string array payloads"
            )
        if "pgy_array_push_String(&result, s)" in body or "pgy_array_push_String(&result, p)" in body:
            raise SystemExit(
                f"{path.relative_to(root)}:{fn} pushes borrowed source strings into result-owned array"
            )

handle_source = root / "src" / "runtime" / "pgy_runtime_lib_slot_array_io_string_exports.h"
handle_text = read_runtime_text(handle_source)
file_open_body = find_function_body(handle_text, "pgy_file_open")
file_close_body = find_function_body(handle_text, "pgy_file_close")
required_handle_terms = [
    "for (int i = 3; i < PGY_MAX_OPEN_FILES; i++)",
    "pgy_runtime_ftable[i] == NULL",
    "fd = i",
    "pgy_runtime_ftable[fd] = fp",
]
missing_handle_terms = [term for term in required_handle_terms if term not in file_open_body]
if missing_handle_terms:
    raise SystemExit(
        "pgy_file_open must reuse closed runtime-owned handle slots; missing "
        + ", ".join(missing_handle_terms)
    )
if "pgy_runtime_ftable[fd] = NULL" not in file_close_body:
    raise SystemExit("pgy_file_close must release the runtime-owned handle slot")

for group_name, (path, macro_name, functions) in macro_exports.items():
    if not path.exists():
        raise SystemExit(f"missing runtime ABI lifetime source: {path.relative_to(root)}")
    text = read_runtime_text(path)
    macro_marker = f"#define {macro_name}"
    macro_index = text.find(macro_marker)
    if macro_index < 0:
        raise SystemExit(f"{path.relative_to(root)} missing {macro_name} macro")
    next_export = text.find(f"\n{macro_name}(", macro_index + len(macro_marker))
    if next_export < 0:
        raise SystemExit(f"{path.relative_to(root)}:{macro_name} has no export invocations")
    macro_body = text[macro_index:next_export]
    if "return result" not in macro_body:
        raise SystemExit(f"{path.relative_to(root)}:{macro_name} has no borrowed return")
    offenders = [
        token for token in forbidden
        if re.search(r"\b" + re.escape(token) + r"\b", macro_body)
    ]
    if offenders:
        raise SystemExit(
            f"{path.relative_to(root)}:{macro_name} performs ownership-changing work: "
            + ", ".join(offenders)
        )
    for fn in functions:
        needle = f"{macro_name}({fn},"
        if needle not in text:
            raise SystemExit(
                f"{path.relative_to(root)} missing borrowed string macro export {fn}"
            )

proof_doc = root / "docs" / "semantics" / "04_ownership_abi.md"
proof_text = proof_doc.read_text(encoding="utf-8")
required_doc_terms = [
    "runtime-borrowed string",
    "result-owned string",
    "result-owned array",
    "runtime-owned handle",
    "caller must not free",
    "caller owns",
    "must eventually release",
    "valid until the next mutation of the corresponding runtime registry",
    "last/history/active/recent",
    "runtime-abi-lifetime-test-smoke",
]
missing = [term for term in required_doc_terms if term not in proof_text]
if missing:
    raise SystemExit(
        "ownership proof doc missing runtime ABI lifetime term(s): "
        + ", ".join(missing)
    )

print("[runtime-abi-lifetime] borrowed exports, result-owned payloads, and file handles are gated")
PY

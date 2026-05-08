#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[memory-string-safety] $*" >&2
    exit 1
}

require_literal() {
    local rel="$1"
    local term="$2"

    grep -Fq -- "$term" "$ROOT_DIR/$rel" ||
        fail "$rel missing memory/string safety term: $term"
}

unsafe_calls="$(
    grep -RInE '\b(sprintf|vsprintf|strcpy|strncpy|strcat|strncat|gets)[[:space:]]*\(' \
        "$ROOT_DIR/src" \
        --include='*.c' --include='*.h' || true
)"
unsafe_calls="$(
    printf '%s\n' "$unsafe_calls" \
        | grep -v '/src/test_' \
        | grep -v '/src/tests/' || true
)"
if [ -n "$unsafe_calls" ]; then
    printf '%s\n' "$unsafe_calls" >&2
    fail "production code must use project-owned bounded formatting/copy/append helpers, not unsafe C string APIs"
fi

truncated_stack_copies="$(
    grep -RInE 'memcpy\([^;]*stack_buf,[[:space:]]*\(size_t\)len[[:space:]]*\+[[:space:]]*1\)' \
        "$ROOT_DIR/src" \
        --include='*.c' --include='*.h' || true
)"
truncated_stack_copies="$(
    printf '%s\n' "$truncated_stack_copies" \
        | grep -v '/src/test_' \
        | grep -v '/src/tests/' || true
)"
if [ -n "$truncated_stack_copies" ]; then
    printf '%s\n' "$truncated_stack_copies" >&2
    fail "snprintf stack buffers must not be copied using the required length after truncation"
fi

unsafe_offset_accumulators="$(
    grep -RInE '\+=[[:space:]]*(\(size_t\))?snprintf\(' \
        "$ROOT_DIR/src" \
        --include='*.c' --include='*.h' || true
)"
unsafe_offset_accumulators="$(
    printf '%s\n' "$unsafe_offset_accumulators" \
        | grep -v '/src/test_' \
        | grep -v '/src/tests/' || true
)"
if [ -n "$unsafe_offset_accumulators" ]; then
    printf '%s\n' "$unsafe_offset_accumulators" >&2
    fail "incremental string assembly must use pergyra_str_append/pergyra_str_appendf, not raw snprintf offset accumulation"
fi

grep -Fq "Memory/string safety audit remains open" "$ROOT_DIR/TODO.md" \
    || fail "TODO must keep the memory/string safety audit bucket visible"
grep -Fq "Buffer Overflow" "$ROOT_DIR/src/test_security_buffer_overflow.c" \
    || fail "historical buffer-overflow regression coverage is missing"
grep -Fq "snprintf" "$ROOT_DIR/src/test_security_comprehensive.c" \
    || fail "bounded formatting regression coverage is missing"

require_literal "src/runtime/pgy_runtime_intent_trace_inline.h" \
    "add_len > ((size_t)-1) - old_len - 1"
require_literal "src/runtime/pgy_runtime_lib_set_intent_trace_exports.c" \
    "add_len > ((size_t)-1) - old_len - 1"
require_literal "src/codegen/llvm_backend_type_map.c" \
    "arg_len > ((size_t)-1) - result_len - 4"
require_literal "src/codegen/llvm_backend_type_map.c" \
    "cur_len > ((size_t)-1) - 2"
require_literal "src/codegen/llvm_stmt_type_render.c" \
    "arg_len > ((size_t)-1) - cur_len - 4"
require_literal "src/codegen/llvm_stmt_type_render.c" \
    "cur_len > ((size_t)-1) - 2"
require_literal "src/codegen/llvm_domain_projection_value_helpers.c" \
    "nested_len > ((size_t)-1) - field_len - 2"
require_literal "src/codegen/llvm_expr_projection_path_helpers.c" \
    "nested_len > ((size_t)-1) - field_len - 2"
require_literal "src/codegen/llvm_member_call_emit.c" \
    "method_len > ((size_t)-1) - class_len - 2"
require_literal "src/codegen/llvm_expr_scalar_core.c" \
    "type_len > ((size_t)-1) - prefix_len - suffix_len - 2"
require_literal "src/semantic/type_checker_class_decl.c" \
    "method_len > ((size_t)-1) - name_len - 2"
require_literal "src/semantic/type_system.c" \
    "inner_len > ((size_t)-1) - prefix_len - 2"
require_literal "src/compiler/path_utils.c" \
    "ext_len > ((size_t)-1) - base_len - 1"

echo "[memory-string-safety] unsafe production string APIs are gated"

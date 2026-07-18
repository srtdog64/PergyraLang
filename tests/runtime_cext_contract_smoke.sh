#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[runtime-cext-contract] $1" >&2
    exit 1
}

require_term() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" \
        || fail "$rel lost required term: $term"
}

reject_term() {
    local rel="$1"
    local term="$2"
    if grep -Fq -- "$term" "$ROOT_DIR/$rel"; then
        fail "$rel regained forbidden term: $term"
    fi
}

reject_term_after_marker() {
    local rel="$1"
    local marker="$2"
    local term="$3"
    if awk -v marker="$marker" 'index($0, marker) { seen=1 } seen { print }' \
        "$ROOT_DIR/$rel" | grep -Fq -- "$term"; then
        fail "$rel regained forbidden term after $marker: $term"
    fi
}

require_program_local_family() {
    local rel="$1"
    local define_term="$2"
    require_term "$rel" "$define_term"
    require_term "$rel" "PGY_RT_PROGRAM_DECL"
    require_term "$rel" "PGY_RT_PROGRAM_BODY"
}

require_term "src/runtime/pgy_runtime_linkage.h" \
    "#define PGY_RT_PROGRAM_DECL static inline"
require_term "src/runtime/pgy_runtime_linkage.h" \
    "#define PGY_RT_PROGRAM_BODY(...) __VA_ARGS__"

# CType/ErrType for these families may be declared only by the generated TU.
# They must never become declarations whose definitions are expected from the
# separately compiled runtime object.
require_program_local_family \
    "src/runtime/pgy_runtime_result_option_inline.h" "#define PGY_RESULT_DEFINE"
require_program_local_family \
    "src/runtime/pgy_runtime_result_option_inline.h" "#define PGY_OPTION_DEFINE"
require_program_local_family \
    "src/runtime/pgy_runtime_list_generic_inline.h" "#define PGY_LIST_DEFINE"
require_program_local_family \
    "src/runtime/pgy_runtime_set_generic_inline.h" "#define PGY_SET_DEFINE"
require_program_local_family \
    "src/runtime/pgy_runtime_builtin_hashmap_inline.h" "#define PGY_HASHMAP_DEFINE"

for rel in \
    src/runtime/pgy_runtime_result_option_inline.h \
    src/runtime/pgy_runtime_list_generic_inline.h \
    src/runtime/pgy_runtime_set_generic_inline.h; do
    reject_term "$rel" "PGY_RT_DECL"
    reject_term "$rel" "PGY_RT_MACRO_BODY"
done
reject_term_after_marker \
    "src/runtime/pgy_runtime_builtin_hashmap_inline.h" \
    "#define PGY_HASHMAP_DEFINE" "PGY_RT_DECL"
reject_term_after_marker \
    "src/runtime/pgy_runtime_builtin_hashmap_inline.h" \
    "#define PGY_HASHMAP_DEFINE" "PGY_RT_MACRO_BODY"

echo "[runtime-cext-contract] program-specialized generic families stay TU-local"

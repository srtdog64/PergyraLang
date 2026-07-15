#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST="$ROOT_DIR/docs/compiler_owner_clusters.tsv"

fail() {
    echo "[compiler-owner-cluster] $*" >&2
    exit 1
}

[[ -f "$MANIFEST" ]] || fail "missing owner cluster manifest"
[[ "$(head -n 1 "$MANIFEST")" == $'cluster_id\troot\tsource\theader\tresponsibility\tstatus' ]] \
    || fail "invalid manifest header"

awk -F '\t' 'NR > 1 { if (seen_id[$1]++ || seen_source[$3]++) exit 1 }' "$MANIFEST" \
    || fail "duplicate cluster id or source"

row_count=0
while IFS=$'\t' read -r cluster_id root source header responsibility status; do
    [[ "$cluster_id" == "cluster_id" ]] && continue
    # actions/checkout may materialize this TSV with CRLF on Windows. Bash
    # `read` retains the final carriage return even though MSYS `head` does
    # not, so normalize only the record terminator before interpreting status.
    status="${status%$'\r'}"
    row_count=$((row_count + 1))
    [[ -n "$cluster_id" && -n "$responsibility" ]] || fail "empty owner row"
    [[ "$status" == "landed" ]] || fail "$cluster_id is not a landed cluster"
    [[ "$source" == "$root/"* && "$header" == "$root/"* ]] \
        || fail "$cluster_id paths escape owner root"
    [[ "$root" != *"/helpers"* && "$source" != *"_helpers."* ]] \
        || fail "$cluster_id uses a generic helpers bucket"
    [[ -f "$ROOT_DIR/$source" && -f "$ROOT_DIR/$header" ]] \
        || fail "$cluster_id source/header missing"
done < "$MANIFEST"

[[ "$row_count" -gt 0 ]] || fail "manifest has no landed owner"
[[ ! -e "$ROOT_DIR/src/codegen/llvm_expr_math_calls.c" ]] \
    || fail "legacy LLVM math source alias remains"
[[ ! -e "$ROOT_DIR/src/codegen/llvm_expr_math_calls.h" ]] \
    || fail "legacy LLVM math header alias remains"
grep -Fq '$(CODEGEN_DIR)/llvm/expression/math_calls.c' "$ROOT_DIR/Makefile" \
    || fail "LLVM math owner is absent from build inventory"
grep -Fq '#include "llvm/expression/math_calls.h"' \
    "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c" \
    || fail "LLVM call dispatch does not consume the physical owner header"
grep -Fq "find '\$(BUILD_DIR)' -type f" "$ROOT_DIR/Makefile" \
    || fail "nested owner artifacts are not cleaned recursively"
grep -Fq 'ALL_DEP_FILES = $(ALL_BUILD_OBJECTS:.o=.d)' "$ROOT_DIR/Makefile" \
    || fail "dependency files are not derived from the object inventory owner"

echo "[compiler-owner-cluster] ${row_count} landed cluster(s); recursive artifacts and legacy-path removal ok"

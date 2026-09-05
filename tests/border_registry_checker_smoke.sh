#!/usr/bin/env bash
# Check the actual runtime-twin include predicate, not compiler semantics.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LABEL=border-registry-checker
fail() { echo "[$LABEL] $*" >&2; exit 1; }
definition="$(awk '
    /^check_runtime_twin_include_boundary\(\) \{/ { capture = 1 }
    capture { print }
    /^}/ { capture = 0 }
' "$ROOT_DIR/tests/border_registry_smoke.sh")"
eval "$definition"
declare -F check_runtime_twin_include_boundary >/dev/null || fail 'missing twin include checker'
mkdir -p "$ROOT_DIR/.tmp/border_registry_checker"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/border_registry_checker/run.XXXXXX")"

for case_name in clean inline_cross extern_cross missing_extern missing_runtime missing_inline; do
    case_dir="$WORK/$case_name"
    mkdir -p "$case_dir"
    if [[ "$case_name" != missing_runtime ]]; then
        printf '%s\n' '#include "shared_decl.h"' >"$case_dir/pgy_runtime.h"
    fi
    printf '%s\n' '/* other inline input remains present */' \
        >"$case_dir/pgy_runtime_other_inline.h"
    if [[ "$case_name" != missing_inline ]]; then
        printf '%s\n' '/* matches pgy_runtime_lib_authority_file_core.h policy */' \
            >"$case_dir/pgy_runtime_panic_checked_inline.h"
    fi
    if [[ "$case_name" != missing_extern ]]; then
        printf '%s\n' '/* twin of pgy_runtime_panic_checked_inline.h */' \
            >"$case_dir/pgy_runtime_lib_authority_file_core.h"
    fi
    case "$case_name" in
        inline_cross)
            printf '%s\n' '# include "pgy_runtime_lib_authority_file_core.h"' \
                >>"$case_dir/pgy_runtime_panic_checked_inline.h"
            expected='inline twin includes the extern twin';;
        extern_cross)
            printf '%s\n' '#include <pgy_runtime_panic_checked_inline.h>' \
                >>"$case_dir/pgy_runtime_lib_authority_file_core.h"
            expected='extern twin includes the inline twin';;
        missing_*) expected='missing runtime twin input';;
    esac
    if output="$(check_runtime_twin_include_boundary "$case_dir" 2>&1)"; then
        [[ "$case_name" == clean ]] || fail "accepted $case_name"
    else
        status=$?
        [[ "$case_name" != clean && "$status" -eq 1 ]] ||
            fail "unexpected $case_name rejection: $output"
        [[ "$output" == *"$expected"* ]] || fail "lost $case_name diagnostic: $output"
    fi
done
echo "[$LABEL] comment-only references accepted; real cross-includes and missing inputs rejected"

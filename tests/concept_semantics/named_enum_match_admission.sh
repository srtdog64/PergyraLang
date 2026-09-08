#!/usr/bin/env bash
# Named-enum coverage is decided before MIR publication. Rejected inputs are
# never compiled or run; complete/default controls also exercise both backends.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here named-enum-match "$PGY"
pgy_require_runnable_binary_here named-enum-match "$DRIVER"
mkdir -p "$ROOT_DIR/.tmp/concept_semantics/named_enum_match"
RUN_DIR="$(mktemp -d "$ROOT_DIR/.tmp/concept_semantics/named_enum_match/run.XXXXXX")"
cd "$ROOT_DIR"
checks=0
failures=0
while IFS='|' read -r source verdict expected; do
    name="$(basename "$source" .pgy)"
    for origin in native public; do
        checks=$((checks + 1))
        stem="$RUN_DIR/$name.$origin"
        command=("$PGY" --native-pipeline --mir-json --error-format=json "$source")
        [[ "$origin" != public ]] || command=("$DRIVER" --emit-mir-json-verified "$source")
        status=0
        timeout 45 "${command[@]}" >"$stem.json" 2>"$stem.err" || status=$?
        if [[ "$verdict" == reject ]]; then
            diagnostic=PGY_SEM_MATCH_PATTERN_INVALID
            [[ "$origin" != public ]] || diagnostic='Code: match_pattern_invalid'
            if [[ "$status" != 1 ]] || grep -Fq '"pgy.mir.v1"' "$stem.json" ||
                ! grep -Fq "$diagnostic" "$stem.json" "$stem.err"; then
                echo "[named-enum-match] FAIL: $name $origin owned refusal ($status)" >&2
                failures=$((failures + 1))
            else
                echo "[named-enum-match] PASS: $name $origin owned refusal, no MIR"
            fi
        elif [[ "$status" != 0 ]] || ! grep -Fq '"pgy.mir.v1"' "$stem.json"; then
            echo "[named-enum-match] FAIL: $name $origin valid admission ($status)" >&2
            failures=$((failures + 1))
        else
            echo "[named-enum-match] PASS: $name $origin valid admission"
        fi
    done
    [[ "$verdict" == accept ]] || continue
    printf '%s\n' "$expected" | tr ',' '\n' >"$RUN_DIR/$name.expected"
    for leg in native.c native.llvm public.c public.llvm; do
        checks=$((checks + 1))
        stem="$RUN_DIR/$name.$leg"
        command=("$PGY" "$source" "--backend=${leg#*.}" --opt=dev -o "$stem.exe")
        [[ "${leg%%.*}" != native ]] || command+=(--native-pipeline)
        status=0
        env -u PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN="$DRIVER" PGY_DEBUG_PIPELINE_TIMING=1 \
            timeout 45 "${command[@]}" >"$stem.compile" 2>&1 || status=$?
        if [[ "$status" != 0 ]] ||
            { [[ "${leg%%.*}" == public ]] && grep -Fq '[pipeline timing]' "$stem.compile"; }; then
            echo "[named-enum-match] FAIL: $name $leg valid compile ($status)" >&2
            failures=$((failures + 1))
            continue
        fi
        timeout 10 "$stem.exe" >"$stem.raw" 2>"$stem.run.err" || status=$?
        tr -d '\r' <"$stem.raw" >"$stem.actual"
        if [[ "$status" != 0 || -s "$stem.run.err" ]] ||
            ! cmp -s "$RUN_DIR/$name.expected" "$stem.actual"; then
            echo "[named-enum-match] FAIL: $name $leg exact execution" >&2
            failures=$((failures + 1))
        else
            echo "[named-enum-match] PASS: $name $leg exact $expected execution"
        fi
    done
done <<'CASES'
tests/concept_semantics/word_deletion/cases/04_match_enum/neg_orig.pgy|reject|
tests/concept_semantics/match/duplicate_missing_rejected.pgy|reject|
tests/concept_semantics/match/nested_missing_rejected.pgy|reject|
tests/concept_semantics/match/default_scope_rejected.pgy|reject|
tests/concept_semantics/match/payload_arity_rejected.pgy|reject|
tests/concept_semantics/match/payload_free_missing_rejected.pgy|reject|
tests/concept_semantics/word_deletion/cases/04_match_enum/orig.pgy|accept|0,4,6
tests/concept_semantics/match/default_valid.pgy|accept|7
tests/concept_semantics/match/default_only_valid.pgy|accept|7
tests/concept_semantics/match/duplicate_complete_valid.pgy|accept|6
tests/concept_semantics/match/non_enum_partial_valid.pgy|accept|4
tests/concept_semantics/match/nested_complete_valid.pgy|accept|6
tests/concept_semantics/match/payload_free_nested_valid.pgy|accept|0,5
tests/concept_semantics/match/payload_free_default_valid.pgy|accept|7
tests/concept_semantics/match/loop_prefix_valid.pgy|accept|9,6
CASES
echo "[named-enum-match] $checks checks / $failures failures; evidence: $RUN_DIR"
[[ "$failures" == 0 ]]

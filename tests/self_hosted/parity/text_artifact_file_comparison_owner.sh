#!/usr/bin/env bash
# Artifact transport may avoid copies; only the real Pergyra comparator decides parity.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL=self-host-text-artifact-file-comparison
fail() { echo "[$LABEL] $*" >&2; exit 1; }
[[ $# -eq 1 && -x "$1" ]] || fail 'expected one already-built Pergyra comparator executable'
COMPARATOR="$1"
mkdir -p "$ROOT_DIR/.tmp/self_hosted/text_artifact_file_comparison"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/text_artifact_file_comparison/run.XXXXXX")"
echo "[$LABEL] artifacts: $WORK"

# Reuse the parent's real Pergyra tool, not a shell substitute for its verdict.
BOUND_COMPARATOR="$(pgy_selfhost_backend_output_comparator_bin "$WORK")"
cp "$COMPARATOR" "$BOUND_COMPARATOR"
cmp -s "$COMPARATOR" "$BOUND_COMPARATOR" || fail 'comparator binding changed bytes'
PGY_SELFHOST_BACKEND_OUTPUT_COMPARATOR_COMPILED_DIRS="$WORK"
EXPECTED_NORM="$WORK/artifact_owner_expected_$$.txt"
ACTUAL_NORM="$WORK/artifact_owner_actual_$$.txt"
OWNER_OUT="$WORK/artifact_owner_compare_$$.out"
LEFT="$WORK/expected artifact.txt"
RIGHT="$WORK/actual artifact.txt"

check_pair() {
    local name="$1" expected="$2" actual="$3" wanted_rc="$4" route="$5"
    local diagnostic="${6:-}" kind="${7:-mir_json}" rc=0 verdict=true
    local expected_owner="$expected" actual_owner="$actual"
    printf '%s\n' 'comparator not reached' >"$OWNER_OUT"
    # The conditional deliberately disables implicit errexit within the subject:
    # its expected/actual normalization guards must reject failures explicitly.
    if (pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL:$name" "$WORK" "$expected" "$actual" "$kind") \
        >"$WORK/$name.out" 2>"$WORK/$name.err"; then
        rc=0
    else
        rc=$?
    fi
    if [[ "$rc" -ne "$wanted_rc" ]]; then
        cat "$WORK/$name.out" "$WORK/$name.err" >&2
        fail "$name expected exit $wanted_rc, got $rc"
    fi
    if [[ -n "$diagnostic" ]]; then
        grep -Fq -- "$diagnostic" "$WORK/$name.err" ||
            fail "$name lost its required diagnostic: $diagnostic"
    fi
    case "$route" in
        raw|normalized)
            if [[ "$route" == normalized ]]; then
                expected_owner="$EXPECTED_NORM"
                actual_owner="$ACTUAL_NORM"
                [[ -f "$EXPECTED_NORM" && -f "$ACTUAL_NORM" ]] ||
                    fail "$name did not materialize required normalization"
            else
                [[ ! -e "$EXPECTED_NORM" && ! -e "$ACTUAL_NORM" ]] ||
                    fail "$name materialized normalized copies of identical input"
            fi
            [[ "$wanted_rc" -eq 0 ]] || verdict=false
            # Match the emitted top-level verdict, not subprocess_plan.ok.
            grep -Fq "{\"schema\":\"pgy.selfhost.backend-output-comparator.v1\",\"ok\":$verdict," "$OWNER_OUT" ||
                fail "$name wrong top-level Pergyra owner verdict"
            grep -Fq "\"expected_owner\":\"${expected_owner#"$ROOT_DIR"/}\"" "$OWNER_OUT" ||
                fail "$name wrong expected transport provenance"
            grep -Fq "\"actual_owner\":\"${actual_owner#"$ROOT_DIR"/}\"" "$OWNER_OUT" ||
                fail "$name wrong actual transport provenance"
            grep -Fq "\"artifact_kind\":\"$kind\"" "$OWNER_OUT" || fail "$name wrong artifact kind"
            grep -Fq '"expected_projection":"c_oracle"' "$OWNER_OUT" ||
                fail "$name lost oracle projection"
            grep -Fq '"actual_projection":"self_hosted"' "$OWNER_OUT" ||
                fail "$name lost self-host projection"
            cp "$OWNER_OUT" "$WORK/$name.owner.json"
            ;;
        unreached)
            grep -Fxq 'comparator not reached' "$OWNER_OUT" ||
                fail "$name reached the comparator after a transport failure"
            ;;
        owner-refusal)
            # The Pergyra owner emits this refusal without a comparison JSON.
            grep -Fq 'BACKEND COMPARATOR OWNER ERROR' "$OWNER_OUT" ||
                fail "$name did not reach the real owner refusal"
            ;;
        *) fail "unknown test route: $route" ;;
    esac
    echo "[$LABEL] $name PASS"
}

# Check no-copy cases before any normalized comparison; input names contain spaces.
for name in identical-lf identical-crlf identical-no-final-lf identical-empty identical-trailing-blank; do
    case "$name" in
        identical-lf) printf 'alpha\nbeta\n' >"$LEFT" ;;
        identical-crlf) printf 'alpha\r\nbeta\r\n' >"$LEFT" ;;
        identical-no-final-lf) printf 'alpha' >"$LEFT" ;;
        identical-empty) printf '' >"$LEFT" ;;
        identical-trailing-blank) printf 'alpha\n\n\n' >"$LEFT" ;;
    esac
    cp "$LEFT" "$RIGHT"
    check_pair "$name" "$LEFT" "$RIGHT" 0 raw
done
check_pair identical-emitted-c "$LEFT" "$RIGHT" 0 raw '' emitted_c
check_pair identical-invalid-kind "$LEFT" "$RIGHT" 1 owner-refusal \
    'BACKEND COMPARATOR OWNER ERROR' not_an_artifact_kind
[[ ! -e "$EXPECTED_NORM" && ! -e "$ACTUAL_NORM" ]] ||
    fail 'owner refusal created normalized copies'

check_pair missing-expected "$WORK/absent expected" "$RIGHT" 1 unreached \
    'artifact input comparison failed (cmp exit 2)'
check_pair missing-actual "$LEFT" "$WORK/absent actual" 1 unreached \
    'artifact input comparison failed (cmp exit 2)'
(
    cmp() { return 7; }
    check_pair byte-compare-error "$LEFT" "$RIGHT" 1 unreached \
        'artifact input comparison failed (cmp exit 7)'
)
(
    cd "$ROOT_DIR"
    check_pair relative-expected-path "${LEFT#"$ROOT_DIR"/}" "$RIGHT" 1 unreached \
        'comparator artifact path escapes repo root'
    check_pair relative-actual-path "$LEFT" "${RIGHT#"$ROOT_DIR"/}" 1 unreached \
        'comparator artifact path escapes repo root'
)
[[ ! -e "$EXPECTED_NORM" && ! -e "$ACTUAL_NORM" ]] ||
    fail 'input errors created normalized copies'

printf 'alpha\nbeta\n' >"$LEFT"
printf 'alpha\r\nbeta\r\n\r\n' >"$RIGHT"
check_pair normalized-crlf-trailing "$LEFT" "$RIGHT" 0 normalized
printf 'alpha' >"$RIGHT"
printf 'alpha\n' >"$LEFT"
check_pair normalized-no-final-lf "$LEFT" "$RIGHT" 0 normalized
ROOT_FORWARD="$(pgy_selfhost_root_forward_slash)"
printf '{"source_module_path":"%s/src/example.pgy","value":1}\n' "$ROOT_FORWARD" >"$LEFT"
printf '{"source_module_path":"src/example.pgy","value":1}\n\n' >"$RIGHT"
check_pair normalized-source-provenance "$LEFT" "$RIGHT" 0 normalized

printf 'alpha\nbeta\n' >"$LEFT"
printf 'alpha\nBETA\n' >"$RIGHT"
check_pair changed-line "$LEFT" "$RIGHT" 1 normalized '"kind":"mismatch"'
printf 'alpha\n\nbeta\n' >"$RIGHT"
check_pair interior-blank "$LEFT" "$RIGHT" 1 normalized '"kind":"mismatch"'
printf '{"other_path":"%s/src/example.pgy"}\n' "$ROOT_FORWARD" >"$LEFT"
printf '{"other_path":"src/example.pgy"}\n' >"$RIGHT"
check_pair other-field-provenance "$LEFT" "$RIGHT" 1 normalized '"kind":"mismatch"'

# Each injected failure emits usable text before failing, so accidental
# continuation could otherwise obtain a green comparison.
printf 'alpha\n' >"$LEFT"
printf 'alpha\r\n' >"$RIGHT"
(
    pgy_selfhost_normalize_text_artifact() { printf 'alpha\n'; return 7; }
    check_pair expected-normalization-error "$LEFT" "$RIGHT" 1 unreached \
        'expected artifact normalization failed'
)
(
    normalization_calls=0
    pgy_selfhost_normalize_text_artifact() {
        normalization_calls=$((normalization_calls + 1))
        printf 'alpha\n'
        [[ "$normalization_calls" -eq 1 ]] || return 7
    }
    check_pair actual-normalization-error "$LEFT" "$RIGHT" 1 unreached \
        'actual artifact normalization failed'
)
(
    pgy_selfhost_root_forward_slash() { return 7; }
    check_pair normalization-root-error "$LEFT" "$RIGHT" 1 unreached \
        'expected artifact normalization failed'
)
(
    pgy_path_for_windows_tool() { return 7; }
    check_pair normalization-root-conversion-error "$LEFT" "$RIGHT" 1 unreached \
        'expected artifact normalization failed'
)
echo "[$LABEL] PASS (real Pergyra verdict, no-copy identity, normalization and explicit refusals)"

#!/usr/bin/env bash
# Checker mechanics only; these synthetic files prove no compiler semantics.
set -euo pipefail
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
POLICY="$REPO_DIR/tests/self_hosted_owner_size_policy.awk"
fail() { echo "[owner-size-policy-checker] $*" >&2; exit 1; }

# BWK awk requires the regexp delimiter escaped inside this bracket expression.
grep -Fq 'rel ~ /^src\/self_hosted\/semantic\/[^\/]+[.]pgy$/' "$POLICY" ||
    fail 'semantic path predicate must escape the slash for BWK awk'

mkdir -p "$REPO_DIR/.tmp/self_hosted/owner_size_policy"
ROOT_DIR="$(mktemp -d "$REPO_DIR/.tmp/self_hosted/owner_size_policy/run.XXXXXX")"
MANIFEST="$ROOT_DIR/tests/fixtures/self_hosted_responsibility_caps.tsv"
mkdir -p "$ROOT_DIR/tests/fixtures" "$ROOT_DIR/src/self_hosted/compiler" \
    "$ROOT_DIR/src/self_hosted/semantic" "$ROOT_DIR/src/self_hosted/codegen/emission"
cp "$POLICY" "$ROOT_DIR/tests/self_hosted_owner_size_policy.awk"
cp "$REPO_DIR/tests/fixtures/self_hosted_responsibility_caps.tsv" "$MANIFEST"

make_lines() { awk -v count="$2" 'BEGIN { for (i = 0; i < count; i++) print "line" }' >"$ROOT_DIR/$1"; }
scan() {
    local target="$1" registered=0 path
    for path in "${owners[@]}"; do [[ "$path" != "$target" ]] || registered=1; done
    (
        cd "$ROOT_DIR"
        if [[ "$registered" -eq 1 ]]; then wc -l "${owners[@]}";
        else wc -l "${owners[@]}" "$target"; fi
    ) | awk -v root="$ROOT_DIR" -v mode=scan \
        -v general_limit="${2:-699}" -v semantic_limit=599 \
        -v general_explicit="${3:-0}" -f "$POLICY"
}
lookup() { awk -v root="$ROOT_DIR" -v mode=lookup -v owner="$1" -f "$POLICY"; }
expect_rejection() {
    local label="$1" diagnostic="$2" status; shift 2
    if ("$@") >"$ROOT_DIR/$label.log" 2>&1; then fail "accepted $label"; else status=$?; fi
    [[ "$status" -eq 1 ]] || fail "$label returned exit $status"
    grep -Fq -- "$diagnostic" "$ROOT_DIR/$label.log" || fail "$label lost diagnostic: $diagnostic"
}

# The real table's values are the test inputs, not another copy of its caps.
owners=()
limits=()
while IFS='|' read -r owner cap; do
    cap="${cap%$'\r'}"
    [[ -z "$owner" || "$owner" == \#* ]] && continue
    owners+=("$owner"); limits+=("$cap")
    make_lines "$owner" 0
done <"$MANIFEST"
[[ "${#owners[@]}" -eq 8 ]] || fail "responsibility migration scope changed"
for ((index = 0; index < ${#owners[@]}; index++)); do
    owner="${owners[$index]}"; cap="${limits[$index]}"
    [[ "$(lookup "$owner")" == "$cap" ]] || fail "lookup drifted: $owner"
    make_lines "$owner" "$cap"
    scan "$owner"
    make_lines "$owner" "$((cap + 1))"
    expect_rejection "over-$index" "cap is $cap" scan "$owner"
    make_lines "$owner" 0
done

generic=src/self_hosted/compiler/unregistered_owner.pgy
semantic=src/self_hosted/semantic/unregistered_owner.pgy
make_lines "$generic" 699; scan "$generic"
make_lines "$generic" 700
expect_rejection generic-over 'cap is 699' scan "$generic"
make_lines "$semantic" 599; scan "$semantic"
make_lines "$semantic" 600
expect_rejection semantic-over 'cap is 599' scan "$semantic"
nested_semantic=src/self_hosted/semantic/nested/unregistered_owner.pgy
mkdir -p "$ROOT_DIR/src/self_hosted/semantic/nested"
make_lines "$nested_semantic" 699; scan "$nested_semantic"
make_lines "$nested_semantic" 700
expect_rejection nested-semantic-over 'cap is 699' scan "$nested_semantic"
lookalike=src/self_hosted/compiler/stmt_emit.pgy
make_lines "$lookalike" 700
expect_rejection basename-lookalike 'cap is 699' scan "$lookalike"
expect_rejection absent-lookup 'missing responsibility cap' lookup "$lookalike"
make_lines "${owners[0]}" 700
expect_rejection explicit-ceiling 'cap is 699' scan "${owners[0]}" 699 1
make_lines "${owners[0]}" 0

# Only registered sources use the component's record-count semantics. Keep
# newline-count semantics for unregistered production sources unchanged.
printf 'a\r\nb' >"$ROOT_DIR/$generic"
scan "$generic" 1 1
expect_rejection newline-over 'cap is 0' scan "$generic" 0 1
printf 'a\r\nb' >"$ROOT_DIR/${owners[0]}"
scan "${owners[0]}" 2 1
expect_rejection registered-unterminated-over 'cap is 1' scan "${owners[0]}" 1 1
make_lines "${owners[0]}" 0
expect_rejection invalid-default 'invalid default size limits' scan "$generic" nope
expect_rejection missing-source 'absent.pgy' scan src/self_hosted/compiler/absent.pgy
partial_scan() {
    printf '1 %s\n' "$generic" | awk -v root="$ROOT_DIR" -v mode=scan \
        -v general_limit=699 -v semantic_limit=599 -v general_explicit=0 -f "$POLICY"
}
expect_rejection partial-inventory 'registered source missing from scan' partial_scan

cp "$MANIFEST" "$ROOT_DIR/original-caps.txt"
printf '%s|%s\n' "${owners[0]}" "${limits[0]}" >>"$MANIFEST"
expect_rejection duplicate-cap 'duplicate responsibility cap' lookup "${owners[0]}"
printf '%s|nope\n' "${owners[0]}" >"$MANIFEST"
expect_rejection invalid-cap 'invalid responsibility cap row' lookup "${owners[0]}"
printf '%s|0800\n' "${owners[0]}" >"$MANIFEST"
expect_rejection noncanonical-cap 'invalid responsibility cap row' lookup "${owners[0]}"
printf 'src/self_hosted/compiler/absent.pgy|1\n' >"$MANIFEST"
expect_rejection registered-missing-source 'unreadable source' lookup "${owners[0]}"
mkdir -p "$ROOT_DIR/src/self_hosted/compiler/directory.pgy"
printf 'src/self_hosted/compiler/directory.pgy|1\n' >"$MANIFEST"
expect_rejection unreadable-source 'unreadable source' lookup "${owners[0]}"
printf '' >"$MANIFEST"
expect_rejection empty-caps 'empty responsibility caps' lookup "${owners[0]}"
expect_rejection missing-caps 'unreadable responsibility caps' \
    awk -v root="$ROOT_DIR" -v mode=lookup -v owner="${owners[0]}" \
        -v manifest="$ROOT_DIR/missing-caps.tsv" -f "$POLICY"
cp "$ROOT_DIR/original-caps.txt" "$MANIFEST"

# Exercise the actual component consumer against the same cap authority.
definition="$(awk '
    /^require_responsibility_owner_max_lines\(\) \{/ { capture = 1 }
    capture { print }
    /^}/ { capture = 0 }
' "$REPO_DIR/tests/self_hosted_component_contract_smoke.sh")"
eval "$definition"
declare -F require_responsibility_owner_max_lines >/dev/null || fail 'missing component consumer'
require_max_lines() { [[ "$1" == "${owners[0]}" && "$2" == "$expected_cap" ]] || fail 'component cap drifted'; }
expected_cap="${limits[0]}"
require_responsibility_owner_max_lines "${owners[0]}"
printf '%s|2\n' "${owners[0]}" >"$MANIFEST"
expected_cap=2
require_responsibility_owner_max_lines "${owners[0]}"
printf '%s|2\n' "${owners[1]}" >"$MANIFEST"
expect_rejection component-missing-cap 'missing or invalid responsibility size authority' \
    require_responsibility_owner_max_lines "${owners[0]}"

echo "[owner-size-policy-checker] exact shared caps, defaults, strict override and missing/malformed authority: PASS"

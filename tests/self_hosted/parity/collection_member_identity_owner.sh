#!/usr/bin/env bash
# Paired source-owned identity units. No input emission, runtime release claim,
# installed-driver promotion, element grant or aggregate SoT closure.
set -Eeuo pipefail
trap 'status=$?; echo "[collection-member-identity] failed at line $LINENO (status $status); evidence: ${REL:-not-created}" >&2' ERR
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here collection-member-identity "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/collection-member-identity.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
PROBE=tests/self_hosted/fixtures/collection_member_identity_probe.pgy
INPUT=tests/self_hosted/parity/fixture/collection_field_lifetime/member_formal_identity_input.pgy
sha256sum "$PGY" >"$WORK/native.sha256"
sha256sum "$PROBE" >"$WORK/member-probe-source.sha256"
sha256sum "$INPUT" >"$WORK/member-input.sha256"
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z | xargs -0 sha256sum >"$WORK/imports.sha256"
PROBE_DIR="${PGY_COLLECTION_MEMBER_PROBE_DIR:-}"
if [[ -n "$PROBE_DIR" ]]; then
    PROBE_DIR="$(cd "$PROBE_DIR" && pwd -P)"
    case "$PROBE_DIR/" in "$ROOT_DIR/.tmp/self_hosted/"*) ;; *) echo 'member probe reuse must stay in this checkout artifact directory' >&2; exit 1 ;; esac
    for manifest in native member-probe-source member-input imports; do
        cmp "$WORK/$manifest.sha256" "$PROBE_DIR/$manifest.sha256"
        sha256sum --quiet -c "$PROBE_DIR/$manifest.sha256"
    done
    sha256sum --binary "$PROBE_DIR/c-member.exe" "$PROBE_DIR/llvm-member.exe" \
        | LC_ALL=C sort >"$WORK/reuse-expected-binaries.sha256"
    while IFS= read -r line || [[ -n "$line" ]]; do
        [[ "${line:0:64}" =~ ^[0-9a-f]{64}$ &&
            ( "${line:64:2}" == ' *' || "${line:64:2}" == '  ' ) ]] || {
            echo 'invalid member probe binary receipt' >&2; exit 1;
        }
        binary_path="$(realpath -- "${line:66}")"
        printf '%s *%s\n' "${line:0:64}" "$binary_path"
    done <"$PROBE_DIR/member-probe-binaries.sha256" \
        | LC_ALL=C sort >"$WORK/reuse-declared-binaries.sha256"
    cmp "$WORK/reuse-expected-binaries.sha256" "$WORK/reuse-declared-binaries.sha256"
    sha256sum --quiet -c "$WORK/reuse-declared-binaries.sha256"
fi
printf 'true\n' >"$WORK/expected"
for backend in c llvm; do
    if [[ -n "$PROBE_DIR" ]]; then
        cp "$PROBE_DIR/$backend-member.exe" "$WORK/$backend-member.exe"
        cmp "$PROBE_DIR/$backend-member.exe" "$WORK/$backend-member.exe"
    else
        timeout 120 "$PGY" --native-pipeline "$PROBE" "--backend=$backend" --opt=dev \
            -o "$REL/$backend-member.exe" >"$WORK/$backend-member.compile" 2>&1
    fi
    sha256sum "$WORK/$backend-member.exe" >>"$WORK/member-probe-binaries.sha256"
    for mutation in {0..44}; do
        timeout 30 "$WORK/$backend-member.exe" "$INPUT" "$mutation" \
            >"$WORK/$backend-$mutation.raw" 2>"$WORK/$backend-$mutation.err"
        tr -d '\r' <"$WORK/$backend-$mutation.raw" >"$WORK/$backend-$mutation.run"
        [[ ! -s "$WORK/$backend-$mutation.err" ]]
        cmp "$WORK/expected" "$WORK/$backend-$mutation.run"
    done
    for mode in garbage 45 -1 01; do
        if "$WORK/$backend-member.exe" "$INPUT" "$mode" >"$WORK/$backend-mode-$mode.raw" 2>"$WORK/$backend-mode-$mode.err"; then
            echo "member observer accepted invalid mode $mode" >&2; exit 1
        else status=$?; fi
        [[ "$status" == 2 && ! -s "$WORK/$backend-mode-$mode.err" ]]
        grep -Fxq 'unknown member mutation' <(tr -d '\r' <"$WORK/$backend-mode-$mode.raw")
    done
    if "$WORK/$backend-member.exe" "$INPUT" >"$WORK/$backend-arity.raw" 2>"$WORK/$backend-arity.err"; then
        echo 'member observer accepted missing mutation' >&2; exit 1
    else status=$?; fi
    [[ "$status" == 2 && ! -s "$WORK/$backend-arity.err" ]]
    grep -Fxq 'expected source and member mutation' <(tr -d '\r' <"$WORK/$backend-arity.raw")
done
for mutation in {0..44}; do cmp "$WORK/c-$mutation.run" "$WORK/llvm-$mutation.run"; done
for manifest in native member-probe-source member-input imports; do sha256sum --quiet -c "$WORK/$manifest.sha256"; done
sha256sum --quiet -c "$WORK/member-probe-binaries.sha256"
echo "[collection-member-identity] C/LLVM each43 member units (baseline +42 negatives),2 constructor-carrier negatives,5 observer refusals PASS; evidence=$REL"

for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" \
        tests/self_hosted/fixtures/collection_member_canonical_scalar_probe.pgy \
        -o "$REL/$backend-canonical-scalar.exe" >"$WORK/$backend-canonical-scalar.compile" 2>&1
    timeout 30 "$WORK/$backend-canonical-scalar.exe" >"$WORK/$backend-canonical-scalar.raw" 2>&1
    tr -d '\r' <"$WORK/$backend-canonical-scalar.raw" >"$WORK/$backend-canonical-scalar.run"
    [[ "$(cat "$WORK/$backend-canonical-scalar.run")" == 'CANONICAL SCALAR COPY PASS' ]]
done
echo "[collection-member-identity] native C/LLVM canonical scalar values after input cleanup PASS; not installed-driver proof"

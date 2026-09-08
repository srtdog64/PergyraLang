#!/usr/bin/env bash
# Same owned refusal + valid execution. Invalid programs are never executed.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here field-write-admission "$PGY"
pgy_require_runnable_binary_here field-write-admission "$DRIVER"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/field-write-admission.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
sha256sum "$PGY" "$DRIVER" >"$WORK/binaries.sha256"
failures=0
checks=0
echo "[field-write-admission] evidence: $REL"
for name in object_write_rejected struct_write_rejected \
    field_write_same_name_rejected field_write_ref_rejected \
    field_write_nested_rejected field_write_nested_object_rejected \
    field_write_tobject_rejected field_write_outer_rejected \
    field_write_nested_ref_rejected; do
    source="tests/concept_semantics/nominal/$name.pgy"
    sha256sum "$source" >>"$WORK/sources.sha256"
    for origin in native self; do
        checks=$((checks + 1))
        status=0
        if [[ "$origin" == native ]]; then
            timeout 30 "$PGY" --native-pipeline --mir-json --error-format=json "$source" \
                >"$WORK/$name.$origin.out" 2>"$WORK/$name.$origin.err" || status=$?
            diagnostic='PGY_SEM_IMMUTABLE_FIELD_WRITE'
        else
            timeout 30 "$DRIVER" --emit-mir-json-verified "$source" \
                >"$WORK/$name.$origin.out" 2>"$WORK/$name.$origin.err" || status=$?
            diagnostic='Code: immutable_field_write'
        fi
        if [[ "$status" != 1 ]] || \
            grep -Fq '"pgy.mir.v1"' "$WORK/$name.$origin.out" || \
            ! grep -Fq "$diagnostic" "$WORK/$name.$origin.out" "$WORK/$name.$origin.err"; then
            echo "[field-write-admission] FAIL $name/$origin: owned refusal absent (status $status)" >&2
            failures=$((failures + 1))
        else
            echo "[field-write-admission] PASS $name/$origin"
        fi
    done
done
while IFS='|' read -r name expected; do
    source="tests/concept_semantics/nominal/$name.pgy"
    sha256sum "$source" >>"$WORK/sources.sha256"
    printf '%s\n' "$expected" >"$WORK/$name.expected"
    for origin in native public; do
      for backend in c llvm; do
        checks=$((checks + 1))
        stem="$name.$origin.$backend"
        command=("$PGY" "$source" "--backend=$backend" --opt=dev -o "$REL/$stem.exe")
        [[ "$origin" == native ]] && command+=(--native-pipeline)
        if ! env -u PGY_NATIVE_PIPELINE PGY_DEBUG_PIPELINE_TIMING=1 \
            PGY_SELF_DRIVER_BIN="$DRIVER" timeout 45 "${command[@]}" \
            >"$WORK/$stem.compile" 2>&1; then
            echo "[field-write-admission] FAIL $stem: valid source rejected" >&2
            failures=$((failures + 1))
            continue
        fi
        if [[ "$origin" == public ]] && grep -Fq '[pipeline timing]' "$WORK/$stem.compile"; then
            echo "[field-write-admission] FAIL $stem: native fallback" >&2
            failures=$((failures + 1))
            continue
        fi
        if ! timeout 10 "$WORK/$stem.exe" >"$WORK/$stem.raw" 2>"$WORK/$stem.err"; then
            echo "[field-write-admission] FAIL $stem: runtime failure" >&2
            failures=$((failures + 1))
            continue
        fi
        tr -d '\r' <"$WORK/$stem.raw" >"$WORK/$stem.run"
        if [[ -s "$WORK/$stem.err" ]] || ! cmp -s "$WORK/$name.expected" "$WORK/$stem.run"; then
            echo "[field-write-admission] FAIL $stem: result mismatch" >&2
            failures=$((failures + 1))
        else
            echo "[field-write-admission] PASS $stem execution"
        fi
      done
    done
done <<'CASES'
field_write_mutable|18
field_write_inout|7
field_write_nested|7
CASES
echo "[field-write-admission] $checks checks / $failures failures"
[[ "$failures" == 0 ]]

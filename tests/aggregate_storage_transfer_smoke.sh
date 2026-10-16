#!/usr/bin/env bash
# Behavioral source boundary for physical collection storage in aggregates.
# Unsafe negative programs are compiled for refusal only, never executed.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
LABEL="aggregate-storage-transfer"
FIXTURE_DIR="tests/self_hosted/parity/fixture/aggregate_storage_transfer"

pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
mkdir -p "$ROOT_DIR/.tmp"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/aggregate-storage-transfer.XXXXXX")"
failures=0

for row in \
    distinct_arrays_positive:3,2 \
    distinct_maps_positive:1,0 \
    empty_descriptor_positive:1,0 \
    explicit_clone_positive:2,1; do
    name="${row%%:*}"
    values="${row#*:}"
    printf '%s\n%s\n' "${values%%,*}" "${values#*,}" >"$WORK_DIR/$name.expected"
    for backend in c llvm; do
        artifact="$WORK_DIR/$name-$backend.exe"
        log="$WORK_DIR/$name-$backend.compile.log"
        if ! (cd "$ROOT_DIR" && "$PGY" --native-pipeline \
            "--backend=$backend" "$FIXTURE_DIR/$name.pgy" -o "$artifact") \
            >"$log" 2>&1; then
            echo "[$LABEL] RED: native $backend rejected positive $name" >&2
            failures=$((failures + 1))
            continue
        fi
        if ! timeout 15 "$artifact" >"$WORK_DIR/$name-$backend.raw" \
            2>"$WORK_DIR/$name-$backend.err"; then
            echo "[$LABEL] RED: native $backend failed positive $name at runtime" >&2
            failures=$((failures + 1))
            continue
        fi
        tr -d '\r' <"$WORK_DIR/$name-$backend.raw" \
            >"$WORK_DIR/$name-$backend.out"
        if [[ -s "$WORK_DIR/$name-$backend.err" ]] || \
            ! cmp -s "$WORK_DIR/$name.expected" \
                "$WORK_DIR/$name-$backend.out"; then
            echo "[$LABEL] RED: native $backend changed positive $name output" >&2
            failures=$((failures + 1))
        fi
    done
done

for name in duplicate_array_negative array_alias_negative \
        map_alias_negative push_pop_duplicate_negative \
        post_field_push_negative post_field_return_negative; do
    for backend in c llvm; do
        artifact="$WORK_DIR/$name-$backend.exe"
        log="$WORK_DIR/$name-$backend.refusal.log"
        status=0
        (cd "$ROOT_DIR" && "$PGY" --native-pipeline \
            "--backend=$backend" --error-format=json \
            "$FIXTURE_DIR/$name.pgy" -o "$artifact") >"$log" 2>&1 || status=$?
        if [[ "$status" -eq 0 ]]; then
            echo "[$LABEL] RED: native $backend admitted unsafe $name; artifact was not run" >&2
            failures=$((failures + 1))
            continue
        fi
        if [[ "$status" -ne 1 ]] || [[ -e "$artifact" ]] || \
            ! python3 - "$log" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    try:
        diagnostics = json.load(stream)
    except json.JSONDecodeError:
        raise SystemExit(1)
if not isinstance(diagnostics, list):
    raise SystemExit(1)
errors = [row for row in diagnostics if isinstance(row, dict) and
          row.get("severity") == "error"]
if not errors:
    raise SystemExit(1)
# A missing-fact refusal is fail-closed but not a completed storage proof.
if errors[0].get("code") not in {
        "PGY_SEM_BORROW_ESCAPE", "PGY_SEM_MOVE_FROM_RELEASED",
        "OWN_EXPLICIT_COPY_REQUIRED"}:
    raise SystemExit(1)
PY
        then
            echo "[$LABEL] RED: native $backend did not give an ownership refusal for $name" >&2
            failures=$((failures + 1))
        fi
    done
done

if [[ "$failures" -ne 0 ]]; then
    echo "[$LABEL] RED: $failures C/LLVM behavioral checks failed; evidence=$WORK_DIR" >&2
    exit 1
fi
echo "[$LABEL] native C/LLVM positive runtime and unsafe-source refusal PASS; evidence=$WORK_DIR"

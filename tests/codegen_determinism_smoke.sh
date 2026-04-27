#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK_ROOT="$ROOT_DIR/.tmp"
mkdir -p "$WORK_ROOT"
WORK_DIR="$(mktemp -d "$WORK_ROOT/pgy_codegen_determinism.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

DEFAULT_PGY="$ROOT_DIR/bin/pgy"
TMP_PGY="${TMPDIR:-/tmp}/pgy-$(basename "$ROOT_DIR")-bin/pgy"
if [[ -x "${DEFAULT_PGY}.exe" ]]; then
    DEFAULT_PGY="${DEFAULT_PGY}.exe"
fi
if [[ -x "${TMP_PGY}.exe" ]]; then
    TMP_PGY="${TMP_PGY}.exe"
fi
if [[ -n "${PGY_BIN:-}" ]]; then
    PGY_BIN="$PGY_BIN"
elif [[ -x "$TMP_PGY" ]]; then
    PGY_BIN="$TMP_PGY"
else
    PGY_BIN="$DEFAULT_PGY"
fi

if [[ ! -x "$PGY_BIN" ]]; then
    echo "[codegen-determinism] missing compiler binary: $PGY_BIN" >&2
    exit 1
fi

DEFAULT_SOURCES=(
    "tests/cases/backend_compare/intent_zone_binding/main.pgy"
    "tests/cases/backend_compare/handoff_projection_frontier/main.pgy"
    "tests/cases/backend_compare/relation_effect_propagation/main.pgy"
    "tests/cases/backend_compare/authority_failure_surface/main.pgy"
)

if [[ "${PGY_CODEGEN_DETERMINISM_SOURCE:-}" == "all" ]]; then
    mapfile -t SOURCES < <(
        cd "$ROOT_DIR"
        find tests/cases/backend_compare -mindepth 2 -maxdepth 2 -name main.pgy | LC_ALL=C sort
    )
elif [[ -n "${PGY_CODEGEN_DETERMINISM_SOURCE:-}" ]]; then
    read -r -a SOURCES <<<"$PGY_CODEGEN_DETERMINISM_SOURCE"
else
    SOURCES=("${DEFAULT_SOURCES[@]}")
fi

normalize_artifact() {
    local input="$1"
    local output="$2"
    sed -E \
        -e 's#\\#/#g' \
        -e 's#ast=0x[0-9A-Fa-f]+#ast=<ptr>#g' \
        -e "s#${WORK_DIR//\\/\\\\}#<workdir>#g" \
        "$input" >"$output"
}

show_diff() {
    local left="$1"
    local right="$2"
    if command -v git >/dev/null 2>&1; then
        git --no-pager diff --no-index -- "$left" "$right" || true
    elif command -v diff >/dev/null 2>&1; then
        diff -u "$left" "$right" || true
    else
        echo "--- $left"
        cat "$left"
        echo "--- $right"
        cat "$right"
    fi
}

sanitize_case_name() {
    printf '%s\n' "$1" | sed -E 's#[^A-Za-z0-9_]+#_#g; s#_main_pgy$##'
}

emit_and_compare() {
    local backend="$1"
    local flag="$2"
    local ext="$3"
    local source_rel="$4"
    local case_name
    local first
    local second
    local first_norm
    local second_norm
    local first_log
    local second_log

    case_name="$(sanitize_case_name "$source_rel")"
    first="$WORK_DIR/${case_name}_${backend}_first.${ext}"
    second="$WORK_DIR/${case_name}_${backend}_second.${ext}"
    first_norm="$WORK_DIR/${case_name}_${backend}_first.norm"
    second_norm="$WORK_DIR/${case_name}_${backend}_second.norm"
    first_log="$WORK_DIR/${case_name}_${backend}_first.log"
    second_log="$WORK_DIR/${case_name}_${backend}_second.log"

    if ! (cd "$ROOT_DIR" && "$PGY_BIN" "$source_rel" "$flag" -o "$first") \
        >"$first_log" 2>&1; then
        echo "[codegen-determinism] first $backend emit failed for $source_rel" >&2
        cat "$first_log" >&2
        return 1
    fi
    if ! (cd "$ROOT_DIR" && "$PGY_BIN" "$source_rel" "$flag" -o "$second") \
        >"$second_log" 2>&1; then
        echo "[codegen-determinism] second $backend emit failed for $source_rel" >&2
        cat "$second_log" >&2
        return 1
    fi

    normalize_artifact "$first" "$first_norm"
    normalize_artifact "$second" "$second_norm"
    if ! cmp -s "$first_norm" "$second_norm"; then
        echo "[codegen-determinism] $backend output is nondeterministic for $source_rel" >&2
        show_diff "$first_norm" "$second_norm" >&2
        return 1
    fi
    echo "[codegen-determinism] PASS $backend ($source_rel)"
}

for source_rel in "${SOURCES[@]}"; do
    if [[ ! -f "$ROOT_DIR/$source_rel" ]]; then
        echo "[codegen-determinism] missing source fixture: $source_rel" >&2
        exit 1
    fi
    emit_and_compare "c" "--emit-c" "c" "$source_rel"
    if [[ "${PGY_CODEGEN_DETERMINISM_BACKENDS:-c llvm}" == *"llvm"* ]]; then
        if "$PGY_BIN" --help 2>&1 | grep -q -- "--emit-llvm"; then
            emit_and_compare "llvm" "--emit-llvm" "ll" "$source_rel"
        else
            echo "[codegen-determinism] SKIP llvm (compiler built without LLVM support)"
        fi
    fi
done

echo "[codegen-determinism] deterministic C/LLVM emit surface ok"

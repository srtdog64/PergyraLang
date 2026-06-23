#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# Test fragments are capped separately from production source. The current
# count is intentional: test fragments are split to keep every .cases.h file
# below the 699 LOC per-fragment gate enforced by test_inc_size_smoke.sh.
MAX_TEST_CASE_INCLUDES="${PGY_MAX_TEST_CASE_INCLUDES:-137}"
violations=()

cd "$ROOT_DIR"

inc_count="$(find src -name '*.inc' -type f | wc -l | tr -d '[:space:]')"
if ((inc_count > 0)); then
    echo ".inc files are not allowed in the beta source tree:" >&2
    find src -name '*.inc' -type f -print >&2
    exit 1
fi

case_count="$(find src/tests -name '*.cases.h' -type f | wc -l | tr -d '[:space:]')"
if ((case_count > MAX_TEST_CASE_INCLUDES)); then
    echo "test case include fragment count increased: ${case_count} > ${MAX_TEST_CASE_INCLUDES}" >&2
    echo "split test case fragments deliberately and update PGY_MAX_TEST_CASE_INCLUDES only with the boundary docs" >&2
    exit 1
fi

production_cases="$(find src -path 'src/tests' -prune -o -name '*.cases.h' -type f -print)"
if [[ -n "$production_cases" ]]; then
    echo ".cases.h fragments are only allowed under src/tests:" >&2
    echo "$production_cases" >&2
    exit 1
fi

case_include_violations=""
tests_real="$(realpath src/tests)"
while IFS= read -r line; do
    rel="${line%%:*}"
    include_path="$(printf '%s\n' "$line" | sed -n 's/.*# *include *"\([^"]*\.cases\.h\)".*/\1/p')"
    [[ -z "$include_path" ]] && continue
    case "$rel" in
        src/test_*.c)
            ;;
        *)
            case_include_violations+="${rel}: .cases.h include is only allowed in test harnesses"$'\n'
            continue
            ;;
    esac
    target_dir="$(dirname "$rel")"
    target_path="${target_dir}/${include_path}"
    if [[ ! -e "$target_path" ]]; then
        case_include_violations+="${rel}: .cases.h include target does not exist: ${include_path}"$'\n'
        continue
    fi
    target_real="$(realpath "$target_path")"
    case "$target_real" in
        "$tests_real"/*)
            ;;
        *)
            case_include_violations+="${rel}: .cases.h include escapes src/tests: ${include_path}"$'\n'
            ;;
    esac
done < <(grep -RIn --include='*.c' --include='*.h' '#[[:space:]]*include[[:space:]]*"[^"]*\.cases\.h"' src Makefile || true)

if [[ -n "$case_include_violations" ]]; then
    echo "invalid test case include usage:" >&2
    echo "$case_include_violations" >&2
    exit 1
fi

while IFS= read -r -d '' path; do
    if [[ ! -s "$path" ]]; then
        violations+=("$path")
    fi
done < <(find src/tests -name '*.cases.h' -type f -print0)

if ((${#violations[@]} > 0)); then
    echo "empty test case include fragments are not allowed:" >&2
    printf '  %s\n' "${violations[@]}" >&2
    exit 1
fi

orphan_cases=""
reference_blob="$(mktemp "${TMPDIR:-/tmp}/pgy-inc-sentinel-refs.XXXXXX")"
trap 'rm -f "$reference_blob"' EXIT
{
    find src -maxdepth 1 -name 'test_*.c' -type f -print0 |
        xargs -0 cat 2>/dev/null || true
    find tests -maxdepth 1 -name '*.sh' -type f -print0 |
        xargs -0 cat 2>/dev/null || true
} > "$reference_blob"

while IFS= read -r -d '' fragment; do
    rel="${fragment#./}"
    name="$(basename "$fragment")"
    if ! grep -Fq -- "$rel" "$reference_blob" 2>/dev/null &&
       ! grep -Fq -- "$name" "$reference_blob" 2>/dev/null; then
        orphan_cases+="${rel}"$'\n'
    fi
done < <(find src/tests -name '*.cases.h' -type f -print0)

if [[ -n "$orphan_cases" ]]; then
    echo "orphan test case include fragments are not allowed:" >&2
    echo "$orphan_cases" >&2
    exit 1
fi

echo "[inc-sentinel] src .inc files = 0; test case include fragments = ${case_count}/${MAX_TEST_CASE_INCLUDES}"

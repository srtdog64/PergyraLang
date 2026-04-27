#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAX_TEST_CASE_INCLUDES="${PGY_MAX_TEST_CASE_INCLUDES:-30}"
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

case_include_violations="$(
python3 - <<'PY'
from pathlib import Path
import re

root = Path(".")
allowed_includers = {
    "src/test_semantic.c",
    "src/test_transpile.c",
}
include_re = re.compile(r'#\s*include\s+"([^"]+\.cases\.h)"')

for path in sorted(root.rglob("*")):
    if not path.is_file():
        continue
    if any(part in {".git", "build"} for part in path.parts):
        continue
    rel = path.relative_to(root).as_posix()
    if path.suffix not in {".c", ".h"} and path.name != "Makefile":
        continue
    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        continue
    for match in include_re.finditer(text):
        include_path = match.group(1)
        if rel not in allowed_includers:
            print(f"{rel}: .cases.h include is only allowed in test harnesses")
            continue
        target = (path.parent / include_path).resolve()
        try:
            target.relative_to((root / "src" / "tests").resolve())
        except ValueError:
            print(f"{rel}: .cases.h include escapes src/tests: {include_path}")
            continue
        if not target.exists():
            print(f"{rel}: .cases.h include target does not exist: {include_path}")
PY
)"

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

orphan_cases="$(
python3 - <<'PY'
from pathlib import Path
root = Path(".")
fragments = sorted((root / "src" / "tests").rglob("*.cases.h"))
reference_paths = [root / "src" / "test_semantic.c", root / "src" / "test_transpile.c"]
reference_paths.extend(sorted((root / "tests").glob("*.sh")))
texts = []
for path in reference_paths:
    if path.exists():
        texts.append(path.read_text(encoding="utf-8", errors="ignore"))

for fragment in fragments:
    rel = fragment.relative_to(root).as_posix()
    name = fragment.name
    if not any(rel in text or name in text for text in texts):
        print(rel)
PY
)"

if [[ -n "$orphan_cases" ]]; then
    echo "orphan test case include fragments are not allowed:" >&2
    echo "$orphan_cases" >&2
    exit 1
fi

echo "[inc-sentinel] src .inc files = 0; test case include fragments = ${case_count}/${MAX_TEST_CASE_INCLUDES}"

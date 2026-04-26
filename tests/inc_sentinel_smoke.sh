#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAX_INC_FILES="${PGY_MAX_INC_FILES:-160}"

violations=()

cd "$ROOT_DIR"

inc_count="$(find src -name '*.inc' -type f | wc -l | tr -d '[:space:]')"
if ((inc_count > MAX_INC_FILES)); then
    echo "source .inc file count increased: ${inc_count} > ${MAX_INC_FILES}" >&2
    echo "new .inc splits are not allowed; move behavior into real .c/.h owners instead" >&2
    exit 1
fi

while IFS= read -r -d '' path; do
    if [[ ! -s "$path" ]]; then
        violations+=("$path")
    fi
done < <(find src -name '*.inc' -type f -print0)

if ((${#violations[@]} > 0)); then
    echo "empty .inc files are not allowed in the beta source tree:" >&2
    printf '  %s\n' "${violations[@]}" >&2
    exit 1
fi

echo "[inc-sentinel] no empty .inc files; source .inc count ${inc_count}/${MAX_INC_FILES}"

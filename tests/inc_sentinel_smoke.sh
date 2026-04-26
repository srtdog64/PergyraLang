#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAX_INC_FILES="${PGY_MAX_INC_FILES:-47}"

violations=()

cd "$ROOT_DIR"

prod_inc_count="$(find src -path 'src/tests' -prune -o -name '*.inc' -type f -print | wc -l | tr -d '[:space:]')"
if ((prod_inc_count > 0)); then
    echo "production .inc files are not allowed in the beta source tree:" >&2
    find src -path 'src/tests' -prune -o -name '*.inc' -type f -print >&2
    exit 1
fi

inc_count="$(find src/tests -name '*.inc' -type f | wc -l | tr -d '[:space:]')"
if ((inc_count > MAX_INC_FILES)); then
    echo "test fixture .inc file count increased: ${inc_count} > ${MAX_INC_FILES}" >&2
    echo "new production .inc splits are not allowed; test fixtures must stay bounded" >&2
    exit 1
fi

while IFS= read -r -d '' path; do
    if [[ ! -s "$path" ]]; then
        violations+=("$path")
    fi
done < <(find src/tests -name '*.inc' -type f -print0)

if ((${#violations[@]} > 0)); then
    echo "empty .inc files are not allowed in the beta source tree:" >&2
    printf '  %s\n' "${violations[@]}" >&2
    exit 1
fi

echo "[inc-sentinel] production .inc files = 0; test fixture .inc count ${inc_count}/${MAX_INC_FILES}"

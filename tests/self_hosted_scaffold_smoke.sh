#!/usr/bin/env bash
# Gates src/self_hosted/ hygiene: compiler artifacts must not land in the
# source tree (build outputs belong under .tmp/self_hosted).

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[self-host-scaffold] $1" >&2
    exit 1
}

artifact_leaks="$(
    cd "$ROOT_DIR" && find src/self_hosted -type f \( \
        -name '*.exe' -o -name '*.o' -o -name '*.obj' -o -name '*.d' \
        -o -name '*.dll' -o -name '*.so' -o -name '*.dylib' \
        -o -name 'probe.*' \
    \) -print
)"
if [[ -n "$artifact_leaks" ]]; then
    printf '%s\n' "$artifact_leaks" >&2
    fail "src/self_hosted/ must not contain compiler artifacts; use .tmp/self_hosted"
fi

echo "[self-host-scaffold] no compiler artifacts under src/self_hosted"

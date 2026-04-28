#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

mapfile -d '' inc_files < <(
    cd "$ROOT_DIR"
    find src/runtime src/codegen src/compiler -name '*.inc' -type f -print0
)

if ((${#inc_files[@]} > 0)); then
    echo "runtime/codegen/compiler production .inc files are not allowed:" >&2
    printf '  %s\n' "${inc_files[@]}" >&2
    exit 1
fi

for legacy_header in \
    rir_builder.h \
    rir_flow.h \
    rir_names.h \
    rir_public_surface.h \
    rir_validation.h
do
    if [ -e "$ROOT_DIR/src/compiler/$legacy_header" ]; then
        echo "RIR implementation-style header reappeared: src/compiler/$legacy_header" >&2
        exit 1
    fi
    if grep -RIn "$legacy_header" "$ROOT_DIR/src" "$ROOT_DIR/Makefile" >/dev/null 2>&1; then
        echo "RIR implementation-style header include/reference remains: $legacy_header" >&2
        grep -RIn "$legacy_header" "$ROOT_DIR/src" "$ROOT_DIR/Makefile" >&2 || true
        exit 1
    fi
done

echo "[backend-inc-size] runtime/codegen/compiler production .inc files = 0; legacy RIR implementation headers = 0"

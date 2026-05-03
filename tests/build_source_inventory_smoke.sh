#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAKE_BIN="${MAKE:-}"

if [[ -z "$MAKE_BIN" ]]; then
    if command -v make >/dev/null 2>&1; then
        MAKE_BIN="$(command -v make)"
    elif command -v mingw32-make >/dev/null 2>&1; then
        MAKE_BIN="$(command -v mingw32-make)"
    else
        echo "[build-source-inventory] missing make" >&2
        exit 1
    fi
fi

inventory="$("$MAKE_BIN" -C "$ROOT_DIR" -s __pgy_build_source_inventory_print)"
missing=0

while IFS= read -r src; do
    [[ -n "$src" ]] || continue

    if [[ ! -f "$ROOT_DIR/$src" ]]; then
        echo "[build-source-inventory] missing required build file: $src" >&2
        missing=1
        continue
    fi

    if command -v git >/dev/null 2>&1 \
        && git -C "$ROOT_DIR" check-ignore -q --no-index "$src" 2>/dev/null; then
        echo "[build-source-inventory] required build file is ignored: $src" >&2
        missing=1
    fi

    if [[ "${PGY_REQUIRE_TRACKED_SOURCES:-0}" == "1" ]] \
        && command -v git >/dev/null 2>&1 \
        && git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1 \
        && ! git -C "$ROOT_DIR" ls-files --error-unmatch "$src" >/dev/null 2>&1; then
        echo "[build-source-inventory] required build file is not tracked: $src" >&2
        missing=1
    fi
done <<< "$inventory"

if command -v git >/dev/null 2>&1 \
    && git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    while IFS= read -r tracked; do
        [[ -n "$tracked" ]] || continue
        if git -C "$ROOT_DIR" ls-files --deleted -- "$tracked" \
            | grep -Fxq "$tracked"; then
            continue
        fi
        [[ -f "$ROOT_DIR/$tracked" ]] || continue

        magic="$(
            LC_ALL=C head -c 4 "$ROOT_DIR/$tracked" 2>/dev/null \
                | od -An -tx1 2>/dev/null \
                | tr -d ' \n'
        )"
        case "$magic" in
            7f454c46|4d5a9000|feedface|feedfacf|cefaedfe|cffaedfe)
                echo "[build-source-inventory] tracked executable artifact is not allowed: $tracked" >&2
                missing=1
                ;;
        esac
    done < <(git -C "$ROOT_DIR" ls-files 'tests/cases' 'examples')
fi

if [[ "$missing" -ne 0 ]]; then
    exit 1
fi

echo "[build-source-inventory] Makefile source inventory ok"

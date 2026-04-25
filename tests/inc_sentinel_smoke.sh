#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

is_allowed_empty_inc() {
    case "$1" in
        src/compiler/mir_public_part_c.inc) return 0 ;;
        src/runtime/pgy_runtime_part_ba_part_f.inc) return 0 ;;
        *) return 1 ;;
    esac
}

shim_for_allowed_empty_inc() {
    case "$1" in
        src/compiler/mir_public_part_c.inc) printf '%s\n' "src/compiler/mir_public.inc" ;;
        src/runtime/pgy_runtime_part_ba_part_f.inc) printf '%s\n' "src/runtime/pgy_runtime_part_ba.inc" ;;
        *) return 1 ;;
    esac
}

violations=()

cd "$ROOT_DIR"

while IFS= read -r -d '' path; do
    if [[ ! -s "$path" ]] && ! is_allowed_empty_inc "$path"; then
        violations+=("$path")
    fi
done < <(find src -name '*.inc' -type f -print0)

if ((${#violations[@]} > 0)); then
    echo "empty .inc files require an explicit beta sentinel allowlist entry:" >&2
    printf '  %s\n' "${violations[@]}" >&2
    exit 1
fi

for allowed in \
    src/compiler/mir_public_part_c.inc \
    src/runtime/pgy_runtime_part_ba_part_f.inc
do
    if [[ ! -f "$allowed" ]]; then
        echo "allowlisted empty .inc sentinel is missing; update the allowlist and shim contract: $allowed" >&2
        exit 1
    fi

    if [[ -s "$allowed" ]]; then
        echo "allowlisted .inc sentinel is no longer empty; remove it from the sentinel allowlist: $allowed" >&2
        exit 1
    fi

    shim="$(shim_for_allowed_empty_inc "$allowed")"
    include_name="$(basename "$allowed")"
    if [[ ! -f "$shim" ]] || ! grep -Fq "#include \"$include_name\"" "$shim"; then
        echo "allowlisted .inc sentinel is not included by its shim: $allowed -> $shim" >&2
        exit 1
    fi
done

echo "[inc-sentinel] empty .inc sentinels are explicit and shim-verified"

#!/usr/bin/env bash
set -eu

root="${1:-$(pwd)}"

artifact_roots() {
    for p in "$root"/.tmp "$root"/build "$root"/bin "$root"/build-* "$root"/bin-*; do
        [ -d "$p" ] || continue
        printf '%s\n' "$p"
    done
}

print_dir_row() {
    dir="$1"
    size_kib="$(du -sk "$dir" 2>/dev/null | awk '{ print $1 }')"
    files="$(find "$dir" -type f 2>/dev/null | wc -l | tr -d ' ')"
    printf '%10s KiB %8s files %s\n' "${size_kib:-0}" "${files:-0}" "$dir"
}

echo "[build-resource] filesystem for $root"
df -Pk "$root" || true
df -Pk "$root" 2>/dev/null | awk '
    NR == 2 {
        used = $5
        sub(/%$/, "", used)
        if ($4 < 10485760 || used >= 95) {
            printf "[build-resource] warning: low free space: %s KiB available, %s%% used; broad local CI may stall\n", $4, used
        }
    }
'

echo "[build-resource] artifact roots"
artifact_roots | sed "s#^$root/#  #"

if [ "${PGY_BUILD_RESOURCE_DEEP:-0}" = "1" ]; then
    echo "[build-resource] artifact root sizes (KiB, files)"
    artifact_roots | while IFS= read -r dir; do
        print_dir_row "$dir"
    done | sort -nr

    echo "[build-resource] .tmp subroots (KiB, files)"
    if [ -d "$root/.tmp" ]; then
        find "$root/.tmp" -mindepth 1 -maxdepth 1 -type d 2>/dev/null |
            while IFS= read -r dir; do
                print_dir_row "$dir"
            done | sort -nr | head -30
    fi
else
    echo "[build-resource] set PGY_BUILD_RESOURCE_DEEP=1 for size and file-count scan"
fi

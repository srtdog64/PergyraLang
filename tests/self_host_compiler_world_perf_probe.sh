#!/usr/bin/env bash
# Local probe for the compiler-world path model. This is intentionally not a CI
# threshold: it prints the cost difference between canonical path lookup and
# recursive discovery so we can decide where to remove scans first.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/self_hosted/compiler_world_manifest.sh"

fail() {
    echo "[self-host-compiler-world-perf] $*" >&2
    exit 1
}

now_ns() {
    local value
    value="$(date +%s%N 2>/dev/null || true)"
    if [[ "$value" =~ ^[0-9]+$ ]]; then
        printf '%s\n' "$value"
        return 0
    fi
    if command -v python3 >/dev/null 2>&1; then
        python3 - <<'PY'
import time
print(time.perf_counter_ns())
PY
        return 0
    fi
    if command -v python >/dev/null 2>&1; then
        python - <<'PY'
import time
try:
    print(time.perf_counter_ns())
except AttributeError:
    print(int(time.perf_counter() * 1000000000))
PY
        return 0
    fi
    fail "no nanosecond timer available"
}

direct_iters="${PGY_COMPILER_WORLD_DIRECT_ITERS:-1000}"
scan_iters="${PGY_COMPILER_WORLD_SCAN_ITERS:-100}"

pgy_compiler_world_require_manifest_paths "$ROOT_DIR" ||
    fail "compiler world path manifest is incomplete"

start="$(now_ns)"
for ((i = 0; i < direct_iters; i++)); do
    pgy_compiler_world_require_manifest_paths "$ROOT_DIR" ||
        fail "missing canonical path during probe"
done
end="$(now_ns)"
direct_ns=$((end - start))

start="$(now_ns)"
scan_count=0
for ((i = 0; i < scan_iters; i++)); do
    scan_count="$(
        find "$ROOT_DIR/src/self_hosted" "$ROOT_DIR/tests/self_hosted" \
            -type f \( -name '*.pgy' -o -name '*.sh' -o -name 'README.md' \) \
            | wc -l \
            | tr -d ' '
    )"
done
end="$(now_ns)"
scan_ns=$((end - start))

direct_avg_ns=$((direct_ns / direct_iters))
scan_avg_ns=$((scan_ns / scan_iters))
ratio_x100=0
if [[ "$direct_avg_ns" -gt 0 ]]; then
    ratio_x100=$(((scan_avg_ns * 100) / direct_avg_ns))
fi

printf '[self-host-compiler-world-perf] canonical_paths=%d direct_iters=%d direct_avg_us=%d scan_iters=%d scan_files=%d scan_avg_us=%d scan_over_direct_x=%.2f\n' \
    "${#PGY_SELFHOST_COMPILER_WORLD_MANIFEST_PATHS[@]}" \
    "$direct_iters" \
    $((direct_avg_ns / 1000)) \
    "$scan_iters" \
    "$scan_count" \
    $((scan_avg_ns / 1000)) \
    "$(awk "BEGIN { printf \"%.2f\", $ratio_x100 / 100 }")"

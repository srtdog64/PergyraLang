#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -n "${CC:-}" ]]; then
    CC_BIN="$CC"
elif command -v cc >/dev/null 2>&1; then
    CC_BIN="cc"
elif command -v gcc >/dev/null 2>&1; then
    CC_BIN="gcc"
elif command -v clang >/dev/null 2>&1; then
    CC_BIN="clang"
else
    echo "[runtime-frontier-policy] missing C compiler: tried CC, cc, gcc, clang" >&2
    exit 1
fi
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT
probe_exe="$tmp_dir/frontier_policy_check.exe"

cat >"$tmp_dir/frontier_policy_check.c" <<'C'
#include <stdint.h>
#include <stddef.h>

#include "codegen/domain_frontier_policy.h"
#include "runtime/pgy_frontier_policy.h"

static int
expect_size(const char *name, size_t actual, size_t expected)
{
    (void)name;
    return actual == expected ? 0 : 1;
}

int
main(void)
{
    size_t cap = (size_t)UINT32_MAX;
    int failures = 0;

    failures += expect_size("cap", pgy_frontier_pass_limit_cap(), cap);
    failures += expect_size("clamp-small", pgy_frontier_pass_limit_clamp(7), 7);
    failures += expect_size("clamp-cap", pgy_frontier_pass_limit_clamp(cap), cap);
    failures += expect_size("add-small", pgy_frontier_pass_limit_add(4, 5), 9);
    failures += expect_size("add-saturates-left", pgy_frontier_pass_limit_add(cap, 1), cap);
    failures += expect_size("add-saturates-right", pgy_frontier_pass_limit_add(cap - 2, 7), cap);
    failures += expect_size("add-one-saturates", pgy_frontier_pass_limit_add_one(cap), cap);
    failures += expect_size("projection-limit-zero", pgy_frontier_projection_pass_limit(0), 1);
    failures += expect_size("projection-limit", pgy_frontier_projection_pass_limit(3), 4);
    failures += expect_size("projection-limit-cap", pgy_frontier_projection_pass_limit(cap), cap);
    failures += expect_size("zone-limit-zero", pgy_frontier_zone_pass_limit(0, 0), 1);
    failures += expect_size("zone-limit", pgy_frontier_zone_pass_limit(2, 3), 6);
    failures += expect_size("zone-limit-cap", pgy_frontier_zone_pass_limit(cap - 1, 9), cap);
    failures += expect_size("world-limit-zero", pgy_frontier_world_pass_limit(0, 0), 1);
    failures += expect_size("world-limit", pgy_frontier_world_pass_limit(2, 4), 7);
    failures += expect_size("world-transitive-limit-zero", pgy_frontier_world_transitive_pass_limit(0, 0, 0), 1);
    failures += expect_size("world-transitive-limit", pgy_frontier_world_transitive_pass_limit(2, 4, 0), 7);
    failures += expect_size("world-transitive-embedded-limit", pgy_frontier_world_transitive_pass_limit(2, 4, 3), 10);
    failures += expect_size("world-transitive-embedded-limit-cap", pgy_frontier_world_transitive_pass_limit(cap - 1, 4, 3), cap);
    failures += expect_size("world-derived-limit-zero", pgy_frontier_world_derived_pass_limit(0), 1);
    failures += expect_size("world-derived-limit", pgy_frontier_world_derived_pass_limit(4), 5);
    failures += expect_size("world-derived-limit-cap", pgy_frontier_world_derived_pass_limit(cap), cap);
    failures += expect_size("domain-zone-null", pgy_domain_zone_frontier_pass_limit(NULL), 1);
    failures += expect_size("domain-projection", pgy_domain_projection_frontier_pass_limit(3), 4);
    failures += expect_size("domain-world-derived-null", pgy_domain_world_derived_frontier_pass_limit(NULL), 1);
    failures += expect_size("domain-world-embedded-null", pgy_domain_world_embedded_frontier_count(NULL, NULL, NULL), 0);
    failures += expect_size("domain-world-transitive-null", pgy_domain_world_transitive_frontier_pass_limit(NULL, 3), 4);
    failures += expect_size("publish-fact-count", PGY_FRONTIER_POLICY_FACT_COUNT, 9);
    failures += expect_size("publish-write-before-ready",
                            pgy_frontier_publish_order_is_valid(
                                PGY_FRONTIER_PUBLISH_WRITE_VALUE,
                                PGY_FRONTIER_PUBLISH_READY),
                            1);
    failures += expect_size("publish-ready-before-clear-dirty",
                            pgy_frontier_publish_order_is_valid(
                                PGY_FRONTIER_PUBLISH_READY,
                                PGY_FRONTIER_PUBLISH_CLEAR_DIRTY),
                            1);
    failures += expect_size("publish-clear-dirty-not-before-ready",
                            pgy_frontier_publish_order_is_valid(
                                PGY_FRONTIER_PUBLISH_CLEAR_DIRTY,
                                PGY_FRONTIER_PUBLISH_READY),
                            0);
    failures += expect_size("publish-invalid-before-rejected",
                            pgy_frontier_publish_order_is_valid(
                                (PgyFrontierPublishPhase)99,
                                PGY_FRONTIER_PUBLISH_READY),
                            0);
    failures += expect_size("publish-invalid-after-rejected",
                            pgy_frontier_publish_order_is_valid(
                                PGY_FRONTIER_PUBLISH_WRITE_VALUE,
                                (PgyFrontierPublishPhase)99),
                            0);

    return failures == 0 ? 0 : 1;
}
C

compiled=0
for candidate in "$CC_BIN" gcc clang cc; do
    command -v "$candidate" >/dev/null 2>&1 || continue
    if "$candidate" -Wall -Wextra -Werror=implicit-function-declaration -Werror=implicit-int \
        -std=c11 -Isrc \
        "$tmp_dir/frontier_policy_check.c" \
        src/codegen/domain_frontier_policy.c \
        -o "$probe_exe"; then
        CC_BIN="$candidate"
        compiled=1
        break
    fi
done
if [[ "$compiled" != "1" ]]; then
    echo "[runtime-frontier-policy] C compiler failed while building frontier policy probe: tried CC/cc/gcc/clang" >&2
    exit 1
fi
if ! "$probe_exe"; then
    echo "[runtime-frontier-policy] frontier policy arithmetic probe failed" >&2
    exit 1
fi

echo "[runtime-frontier-policy] bounded frontier pass-limit arithmetic is gated"

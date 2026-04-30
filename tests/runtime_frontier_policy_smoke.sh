#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CC_BIN="${CC:-cc}"
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

cat >"$tmp_dir/frontier_policy_check.c" <<'C'
#include <stdint.h>
#include <stddef.h>

#include "codegen/domain_frontier_policy.h"

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
    failures += expect_size("world-transitive-limit-zero", pgy_frontier_world_transitive_pass_limit(0, 0), 1);
    failures += expect_size("world-transitive-limit", pgy_frontier_world_transitive_pass_limit(2, 4), 7);
    failures += expect_size("world-derived-limit-zero", pgy_frontier_world_derived_pass_limit(0), 1);
    failures += expect_size("world-derived-limit", pgy_frontier_world_derived_pass_limit(4), 5);
    failures += expect_size("world-derived-limit-cap", pgy_frontier_world_derived_pass_limit(cap), cap);

    return failures == 0 ? 0 : 1;
}
C

"$CC_BIN" -Wall -Wextra -Werror=implicit-function-declaration -Werror=implicit-int \
    -std=c11 -Isrc "$tmp_dir/frontier_policy_check.c" -o "$tmp_dir/frontier_policy_check"
"$tmp_dir/frontier_policy_check"

echo "[runtime-frontier-policy] bounded frontier pass-limit arithmetic is gated"

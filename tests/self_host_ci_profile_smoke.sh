#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CHANGE_SCOPE_OWNER="$ROOT_DIR/scripts/ci_change_scope_owner.sh"

scope_tmp="$(mktemp -d)"
trap 'rm -rf "$scope_tmp"' EXIT
scope_repo="$scope_tmp/repo"
mkdir -p "$scope_repo"
git -C "$scope_repo" init -q
git -C "$scope_repo" config user.name ci-scope-test
git -C "$scope_repo" config user.email ci-scope-test@example.invalid
git -C "$scope_repo" config core.autocrlf false
printf '# baseline\n' >"$scope_repo/README.md"
printf 'int baseline;\n' >"$scope_repo/source.c"
git -C "$scope_repo" add README.md source.c
git -C "$scope_repo" commit -qm baseline
scope_base="$(git -C "$scope_repo" rev-parse HEAD)"

assert_scope() {
    local base="$1"
    local head="$2"
    local expected_full="$3"
    local expected_markdown="$4"
    local expected_reason="$5"
    local output="$scope_tmp/output"
    : >"$output"
    (
        cd "$scope_repo"
        bash "$CHANGE_SCOPE_OWNER" "$base" "$head" "$output" >/dev/null
    )
    for expected in \
        "run_full=$expected_full" \
        "markdown_only=$expected_markdown" \
        "reason=$expected_reason"; do
        if ! grep -Fxq "$expected" "$output"; then
            echo "[self-host-ci-profile] change-scope result mismatch: $expected" >&2
            exit 1
        fi
    done
}

printf '# markdown only\n' >>"$scope_repo/README.md"
git -C "$scope_repo" add README.md
git -C "$scope_repo" commit -qm markdown-only
scope_markdown="$(git -C "$scope_repo" rev-parse HEAD)"
assert_scope "$scope_base" "$scope_markdown" false true markdown-only

printf '# mixed\n' >>"$scope_repo/README.md"
printf 'int changed;\n' >>"$scope_repo/source.c"
git -C "$scope_repo" add README.md source.c
git -C "$scope_repo" commit -qm mixed
scope_mixed="$(git -C "$scope_repo" rev-parse HEAD)"
assert_scope "$scope_markdown" "$scope_mixed" true false non-markdown-change

git -C "$scope_repo" mv source.c source.md
git -C "$scope_repo" commit -qm rename-into-markdown
scope_rename="$(git -C "$scope_repo" rev-parse HEAD)"
assert_scope "$scope_mixed" "$scope_rename" true false non-markdown-change
assert_scope 0000000000000000000000000000000000000000 "$scope_rename" true false base-unavailable
assert_scope "$scope_rename" "$scope_rename" true false empty-diff

echo "[self-host-ci-profile] exhaustive parity, bootstrap, and native platform jobs are isolated"

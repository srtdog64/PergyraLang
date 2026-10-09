#!/usr/bin/env bash
# The receipt's Git context trusts exactly the authenticated checkout.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
fail() { echo "[rocq-cost-git-context] $*" >&2; exit 1; }
[[ "${GIT_CONFIG_COUNT:-}" == 2 &&
   "${GIT_CONFIG_KEY_0:-}" == safe.directory &&
   "${GIT_CONFIG_VALUE_0+x}" == x && -z "$GIT_CONFIG_VALUE_0" &&
   "${GIT_CONFIG_KEY_1:-}" == safe.directory &&
   -n "${GIT_CONFIG_VALUE_1:-}" && "$GIT_CONFIG_VALUE_1" != *'*'* ]] ||
    fail 'explicit checkout-only protected Git context is required'
mkdir -p "$ROOT_DIR/.tmp"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/rocq-cost-git-context.XXXXXX")"
git --no-optional-locks -C "$ROOT_DIR" rev-parse HEAD >"$WORK/head.expected"
GIT_TEST_ASSUME_DIFFERENT_OWNER=1 git --no-optional-locks -C "$ROOT_DIR" \
    rev-parse HEAD >"$WORK/head.admitted" 2>"$WORK/head.err" ||
    fail 'approved checkout refused the forced ownership check'
cmp "$WORK/head.expected" "$WORK/head.admitted" || fail 'checkout HEAD drift'
[[ ! -s "$WORK/head.err" ]] || fail 'approved checkout emitted Git error'
git init --quiet "$WORK/unrelated"
if GIT_TEST_ASSUME_DIFFERENT_OWNER=1 git --no-optional-locks \
    -C "$WORK/unrelated" status --porcelain \
    >"$WORK/unrelated.out" 2>"$WORK/unrelated.err"; then
    fail 'unrelated repository bypassed ownership refusal'
else
    refusal=$?
fi
[[ "$refusal" == 128 ]] || fail "unrelated refusal status is $refusal, not 128"
grep -Fq 'detected dubious ownership' "$WORK/unrelated.err" ||
    fail 'unrelated refusal was not the ownership boundary'
GIT_TEST_ASSUME_DIFFERENT_OWNER=1 GIT_CONFIG_VALUE_1="$WORK/unrelated" \
    git --no-optional-locks -C "$WORK/unrelated" status --porcelain \
    >"$WORK/fixture.admitted" 2>"$WORK/fixture.err" ||
    fail 'explicitly approved fixture refused'
[[ ! -s "$WORK/fixture.admitted" && ! -s "$WORK/fixture.err" ]] ||
    fail 'approved empty fixture status changed'
echo '[rocq-cost-git-context] checkout admission, unrelated refusal and explicit fixture control PASS'

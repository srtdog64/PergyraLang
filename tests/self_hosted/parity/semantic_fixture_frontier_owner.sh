#!/usr/bin/env bash
# Transport/negative gate for the existing Pergyra-owned verdict frontier.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
PARITY="$ROOT_DIR/tests/self_hosted/parity/semantic_parity.sh"
LABEL=self-host-semantic-fixture-frontier
fail() { echo "[$LABEL] $*" >&2; exit 1; }

definition="$(awk '
    /^check_semantic_fixture_frontier\(\) \{/ { capture = 1 }
    capture { print }
    /^}/ { capture = 0 }
' "$PARITY")"
eval "$definition"
declare -F check_semantic_fixture_frontier >/dev/null || fail 'missing frontier consumer'

SOURCE_PAIRS=('valid:ok::' 'invalid:error:owned_code:PGY_OWNED_CODE')
check_semantic_fixture_frontier 2 || fail 'matching owner frontier rejected'
for bad in '' 0 -1 02 text $'2\n2' $'2\r' 1 3 999999999999999999999999; do
    if output="$(check_semantic_fixture_frontier "$bad" 2>&1)"; then
        fail "accepted missing, malformed or mismatched frontier: $bad"
    else
        status=$?
    fi
    [[ "$status" -eq 1 ]] || fail "unexpected rejection status: $status"
    case "$output" in
        *'malformed owner fixture frontier:'*|*'fixture manifest count drifted:'*) ;;
        *) fail "lost frontier diagnostic: $output" ;;
    esac
done
SOURCE_PAIRS=()
if check_semantic_fixture_frontier 2 >/dev/null 2>&1; then
    fail 'empty parsed manifest accepted'
fi
echo "[$LABEL] matching owner frontier and missing/malformed/mismatched refusals PASS"

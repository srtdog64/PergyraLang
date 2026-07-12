#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

fail() {
    echo "[self-host-source-scan] $*" >&2
    exit 1
}

require_text() {
    local file="$1" text="$2"
    grep -Fq "$text" "$file" || fail "missing '$text' in $file"
}

reject_region_text() {
    local file="$1" start="$2" text="$3"
    local region
    region="$(sed -n "/$start/,/^}/p" "$file")"
    if grep -Fq "$text" <<<"$region"; then
        fail "forbidden '$text' returned in $file::$start"
    fi
}

SOURCE_OWNER="src/self_hosted/lib/source_scan_owner.pgy"
PARSER_CURSOR="src/self_hosted/parser/cursor_owner.pgy"
SEMANTIC_SCAN="src/self_hosted/semantic/text_scan_owner.pgy"

for term in \
    "func SourceByteAt" \
    "func SourceByteOf" \
    "func SourceByteIsAlpha" \
    "func SourceByteIsDigit" \
    "func SourceByteIsAlphaNum" \
    "func SourceByteIsWhitespace"; do
    require_text "$SOURCE_OWNER" "$term"
done

require_text "$SOURCE_OWNER" \
    "let c: Int = SourceByteAt(content, n, i);"
reject_region_text "$SOURCE_OWNER" \
    "func SkipWhitespaceAndComments" "SourceCharAt("
reject_region_text "$SOURCE_OWNER" \
    "func SkipWhitespaceAndComments" "Substring("

for function in ReadIdent MatchKeyword ReadNumber ReadString ExpectOpt \
    ConsumeStmtTerminatorOpt; do
    reject_region_text "$PARSER_CURSOR" "func $function" "ParserCharAt("
done
reject_region_text "$PARSER_CURSOR" "func MatchKeyword" "Substring("
reject_region_text "$PARSER_CURSOR" "func ExpectOpt" "Substring("
require_text "$PARSER_CURSOR" \
    "SourceByteIsAlphaNum(SourceByteAt(content, n, i))"
require_text "$PARSER_CURSOR" \
    "SubEqualsWithLen(content, n, i, kl, kw)"

if grep -Fq "CharAt(content" "$SEMANTIC_SCAN"; then
    fail "semantic scanner reopened allocating character reads"
fi
require_text "$SEMANTIC_SCAN" \
    "SourceByteAt(content, limit, i)"
require_text "$SEMANTIC_SCAN" \
    "SubEqualsWithLen(content, n, i, kl, kw)"

EVIDENCE="benchmarks/selfhost_source_scan_owner_evidence.json"
owner_hash="$({
    for file in \
        "$SOURCE_OWNER" \
        "$PARSER_CURSOR" \
        "$SEMANTIC_SCAN"; do
        printf '%s:' "$file"
        git hash-object "$file"
    done
} | sha256sum | awk '{ print toupper($1) }')"
require_text "$EVIDENCE" "\"owner_set_sha256\": \"$owner_hash\""
require_text "$EVIDENCE" '"parser_fixtures": 188'
require_text "$EVIDENCE" '"semantic_fixtures": 110'
require_text "$EVIDENCE" '"integrated_driver_c_llvm_byte_identical": true'

baseline_min="$(sed -n '/"baseline"/,/}/s/.*"elapsed_ms": \[\(.*\)\].*/\1/p' "$EVIDENCE" |
    tr ',' '\n' | awk 'BEGIN { min = 999999999 } { gsub(/ /, ""); if ($1 + 0 < min) min = $1 + 0 } END { print min }')"
candidate_max="$(sed -n '/"byte_scan_owner"/,/}/s/.*"elapsed_ms": \[\(.*\)\].*/\1/p' "$EVIDENCE" |
    tr ',' '\n' | awk 'BEGIN { max = 0 } { gsub(/ /, ""); if ($1 + 0 > max) max = $1 + 0 } END { print max }')"
required_max="$(sed -n '/"byte_scan_owner"/,/}/s/.*"required_max_elapsed_ms": \([0-9]*\).*/\1/p' "$EVIDENCE")"
awk -v baseline="$baseline_min" -v candidate="$candidate_max" \
    -v required="$required_max" \
    'BEGIN { exit !(candidate < baseline && candidate <= required) }' ||
    fail "source-scan benchmark evidence relationships drifted"

echo "[self-host-source-scan] byte/code owner, parity evidence, and allocation-free hot scans ok"

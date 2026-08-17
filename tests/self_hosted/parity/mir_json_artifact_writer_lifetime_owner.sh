#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
JSON_EMIT="$ROOT_DIR/src/self_hosted/lib/json_emit.pgy"
PROGRAM_WRITER="$ROOT_DIR/src/self_hosted/mir/program_json_artifact_writer_owner.pgy"
INSTRUCTION_WRITER="$ROOT_DIR/src/self_hosted/mir/instruction_json_artifact_writer_owner.pgy"
IDENTITY_WRITER="$ROOT_DIR/src/self_hosted/mir/expression_identity_json_projection_owner.pgy"
LOCAL_REF_WRITER="$ROOT_DIR/src/self_hosted/mir/local_ref_json_projection_owner.pgy"

fail() { echo "[mir-json-writer-lifetime] $*" >&2; exit 1; }
require() { grep -Fq "$2" "$1" || fail "$1 missing: $2"; }
reject() { ! grep -Fq "$2" "$1" || fail "$1 retained: $2"; }

require "$JSON_EMIT" 'func JsonOwnedFragmentWriteFile('
require "$JSON_EMIT" 'ArrayDropOwnedStrings(owned_fragment);'
require "$JSON_EMIT" 'func JsonEscapeTokenAt('
require "$JSON_EMIT" 'if !JsonStringLiteralRequiresEscape(value, value_length) {'
require "$JSON_EMIT" 'CompilerArtifactWrite(output, value);'
require "$IDENTITY_WRITER" 'SelfMirJsonExpressionIdentityProjectionForNode(graph, node)'
require "$IDENTITY_WRITER" 'JsonStringLiteralWriteFile(output, projection.binding_kind)'
identity_write_body="$(awk '
    /^func SelfMirJsonExpressionIdentityWriteFile\(/ { active = 1 }
    active { print; seen = 1 }
    active && /^}/ { exit }
' "$IDENTITY_WRITER")"
[[ "$identity_write_body" != *'SelfMirJsonExpressionIdentityFields('* ]] ||
    fail "identity artifact writer rebuilt the String field array"
[[ "$identity_write_body" != *'JsonEmitField'* ]] ||
    fail "identity artifact writer rebuilt JSON field fragments"
reject "$LOCAL_REF_WRITER" 'CompilerArtifactWrite(output, ToString('

for writer in "$PROGRAM_WRITER" "$INSTRUCTION_WRITER"; do
    reject "$writer" 'CompilerArtifactWrite(output, ToString('
    reject "$writer" 'CompilerArtifactWrite(output, JsonEmit'
    reject "$writer" 'CompilerArtifactWrite(output, SelfMirJson'
done

# String is copy-only today, so `own fragment` alone cannot reject a borrowed
# fact String. Parse calls across line wrapping and admit only renderer/number
# results plus the verified non-empty ABI-layout local.
calls="$(awk '
    /JsonOwnedFragmentWriteFile[[:space:]]*\(/ { collecting = 1 }
    collecting {
        text = text " " $0; opens = $0; closes = $0
        gsub(/[^(]/, "", opens); gsub(/[^)]/, "", closes)
        balance += length(opens) - length(closes)
        if (balance == 0) {
            gsub(/[[:space:]]+/, " ", text); sub(/^ /, "", text)
            print text; text = ""; collecting = 0
        }
    }
' "$PROGRAM_WRITER" "$INSTRUCTION_WRITER" "$IDENTITY_WRITER" "$LOCAL_REF_WRITER")"
while IFS= read -r call; do
    [[ -n "$call" ]] || continue
    case "$call" in
        *'ToString('*|*'JsonEmitField'*|*'JsonEmitObject('*) ;;
        *'SelfMirJson'*|*'SelfMirDomain'*|*'output, abi_layout);'*) ;;
        *) fail "non-renderer owned-fragment argument: $call" ;;
    esac
done <<<"$calls"

echo "[mir-json-writer-lifetime] owned fragments, escape SoT, and borrowed-fact exclusion PASS"

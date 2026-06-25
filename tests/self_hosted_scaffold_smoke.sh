#!/usr/bin/env bash
# Gates the src/self_hosted/ scaffold integrity.
#
# This smoke does not run the Pergyra tools or the parity rungs (each tool
# owns its own parity script under tests/self_hosted/parity/). It only verifies the
# scaffold contract from docs/self_hosted/00_agent_entry.md is intact:
#
#   - legacy root self_hosted/ is absent; executable scaffolds live in src/self_hosted/
#   - src/self_hosted/README.md exists
#   - tests/self_hosted/parity/README.md exists
#   - every tool dir under src/self_hosted/tools/<name>/ has intent.md and main.pgy
#   - every tool dir has a matching parity script tests/self_hosted/parity/<name>_parity.sh
#   - every parity script is bash-compatible and declares `set -euo pipefail`

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LEGACY_SELF_HOST_DIR="$ROOT_DIR/self_hosted"
SELF_HOST_DIR="$ROOT_DIR/src/self_hosted"
TOOLS_DIR="$SELF_HOST_DIR/tools"
PARITY_DIR="$ROOT_DIR/tests/self_hosted/parity"

fail() {
    echo "[self-host-scaffold] $1" >&2
    exit 1
}

[[ ! -d "$LEGACY_SELF_HOST_DIR" ]] \
    || fail "legacy root self_hosted/ directory must stay absent; use src/self_hosted/"
[[ -d "$SELF_HOST_DIR" ]] || fail "missing src/self_hosted/ directory"
[[ ! -d "$SELF_HOST_DIR/parity" ]] \
    || fail "src/self_hosted/parity must stay absent; parity harnesses live in tests/self_hosted/parity/"
[[ -f "$SELF_HOST_DIR/README.md" ]] || fail "missing src/self_hosted/README.md"
[[ -f "$ROOT_DIR/tests/self_hosted/README.md" ]] || fail "missing tests/self_hosted/README.md"
[[ -f "$PARITY_DIR/README.md" ]] || fail "missing tests/self_hosted/parity/README.md"
[[ -f "$SELF_HOST_DIR/PROGRESS.md" ]] || fail "missing src/self_hosted/PROGRESS.md (self-host coverage tracker)"
grep -Fq '`src/self_hosted/` is for Pergyra source owners.' "$ROOT_DIR/tests/self_hosted/README.md" \
    || fail "tests/self_hosted/README.md must define src/tests ownership split"
grep -Fq 'tests/self_hosted/' "$SELF_HOST_DIR/README.md" \
    || fail "src/self_hosted/README.md must point parity/test artifacts to tests/self_hosted/"

# PROGRESS.md must contain the canonical Headline Number anchor so
# updates are not silently dropped during edits.
if ! grep -Fq 'Headline Number' "$SELF_HOST_DIR/PROGRESS.md"; then
    fail "src/self_hosted/PROGRESS.md missing 'Headline Number' section"
fi
if ! grep -Fq 'Component Coverage' "$SELF_HOST_DIR/PROGRESS.md"; then
    fail "src/self_hosted/PROGRESS.md missing 'Component Coverage' section"
fi

artifact_leaks="$(
    cd "$ROOT_DIR" && find src/self_hosted -type f \( \
        -name '*.exe' -o -name '*.o' -o -name '*.obj' -o -name '*.d' \
        -o -name '*.dll' -o -name '*.so' -o -name '*.dylib' \
        -o -name 'probe.*' \
    \) -print
)"
if [[ -n "$artifact_leaks" ]]; then
    printf '%s\n' "$artifact_leaks" >&2
    fail "src/self_hosted/ must not contain compiler artifacts; use .tmp/self_hosted"
fi

if [[ ! -d "$TOOLS_DIR" ]]; then
    echo "[self-host-scaffold] no tools yet (tools/ absent); scaffold ok"
    exit 0
fi

tool_count=0
for tool_path in "$TOOLS_DIR"/*/; do
    [[ -d "$tool_path" ]] || continue
    tool_name="$(basename "$tool_path")"
    tool_count=$((tool_count + 1))

    [[ -f "$tool_path/intent.md" ]] \
        || fail "tool '$tool_name' missing intent.md"
    [[ -f "$tool_path/main.pgy" ]] \
        || fail "tool '$tool_name' missing main.pgy"

    # intent.md must declare the four contract anchors. During soft self-host,
    # the C checker remains the oracle and Pergyra tools are parity candidates.
    for anchor in '## Intent' '## Input Contract' '## Output Contract'; do
        if ! grep -Fq -- "$anchor" "$tool_path/intent.md"; then
            fail "tool '$tool_name' intent.md missing section: $anchor"
        fi
    done
    if ! grep -Fq -- '## Oracle' "$tool_path/intent.md"; then
        fail "tool '$tool_name' intent.md missing section: ## Oracle"
    fi

    parity_script="$PARITY_DIR/${tool_name}_parity.sh"
    [[ -f "$parity_script" ]] \
        || fail "tool '$tool_name' missing parity script: ${tool_name}_parity.sh"
    grep -Fq 'set -euo pipefail' "$parity_script" \
        || fail "parity script '$parity_script' missing 'set -euo pipefail'"
done

if [[ "$tool_count" -eq 0 ]]; then
    echo "[self-host-scaffold] tools/ present but empty; scaffold ok"
    exit 0
fi

echo "[self-host-scaffold] $tool_count tool(s) gated"

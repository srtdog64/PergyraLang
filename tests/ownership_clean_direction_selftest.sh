#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
bash "$ROOT_DIR/tests/ownership_clean_direction_smoke.sh"
real_grep="$(command -v grep)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
for status in 0 2; do
    # Simulate a residue match, then a search-tool failure. Neither may be a
    # successful no-match. The real source/core is never changed by the test.
    printf '#!/usr/bin/env bash\nexit %s\n' "$status" > "$work/grep"
    chmod +x "$work/grep"
    if PATH="$work:$PATH" bash "$ROOT_DIR/tests/ownership_clean_direction_smoke.sh" >"$work/reject.log" 2>&1; then
        echo "direction gate accepted search status $status" >&2; exit 1
    fi
    if [[ "$status" = 0 ]]; then
        "$real_grep" -Fq 'dangling/retired source owner reference' "$work/reject.log"
    else
        "$real_grep" -Fq 'retired-owner search failed (2)' "$work/reject.log"
    fi
done
echo '[ownership-clean-direction-selftest] match and tool/I/O failure refused; no source/model mutation'

#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
POSITIONING_DOC="$ROOT_DIR/docs/114_async_model_positioning.md"
GUIDE_DOC="$ROOT_DIR/docs/05_async_concurrency.md"
CONTRACT_DOC="$ROOT_DIR/docs/113_memory_concurrency_model.md"

for path in "$POSITIONING_DOC" "$GUIDE_DOC" "$CONTRACT_DOC"; do
    if [[ ! -f "$path" ]]; then
        echo "[async-positioning] missing doc: $path" >&2
        exit 1
    fi
done

for required in \
    "coloring decomposition" \
    "not \"avoid coloring\"" \
    "Each concern has an owner" \
    "Widening one cell must not silently widen the others" \
    "await is a completion join for checked futures" \
    "Future<T> and RemoteFuture<T> are typed completion handles" \
    "not a general user-level effect system" \
    "visibility-high / decomposition-high" \
    "AIR Phase 1 sync/async drift detection" \
    "explicit named task creation plus" \
    "Capture-bearing detached async blocks as the stable task creation model"; do
    if ! grep -Fq "$required" "$POSITIONING_DOC"; then
        echo "[async-positioning] positioning doc missing: $required" >&2
        exit 1
    fi
done

for required in \
    "Pergyra does not treat \`async\` as the umbrella concept" \
    "\`await\` only owns completion join" \
    "RemoteFuture<T> -> await -> Result<T>" \
    "Use named \`spawn Worker(args...)\` for beta-stable task creation" \
    "Pinned views and slot views cannot cross suspension" \
    "Do not read Pergyra as \"async without coloring.\""; do
    if ! grep -Fq "$required" "$GUIDE_DOC"; then
        echo "[async-positioning] guide doc missing: $required" >&2
        exit 1
    fi
done

for forbidden in \
    "coloring avoidance" \
    "hides suspension" \
    "async is the umbrella"; do
    if grep -Fq "$forbidden" "$POSITIONING_DOC"; then
        echo "[async-positioning] positioning doc contains forbidden simplification: $forbidden" >&2
        exit 1
    fi
done

if ! grep -Fq "docs/114_async_model_positioning.md" "$CONTRACT_DOC"; then
    echo "[async-positioning] contract doc must link positioning doc" >&2
    exit 1
fi

echo "[async-positioning] coloring decomposition docs ok"

#!/usr/bin/env bash
# Supported-contract regression only. Source admission parity has a separate
# failing entrypoint; a green result here does not mean self-host closure.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
for runner in \
    intent/terminal_substitution_smoke.sh \
    authority_effect/run.sh \
    domain_axes/run.sh \
    nominal/run.sh; do
    timeout 300 bash "$ROOT_DIR/tests/concept_semantics/$runner"
done
echo '[concept-semantics] 4 bounded regression lanes: PASS (not source-admission closure)'

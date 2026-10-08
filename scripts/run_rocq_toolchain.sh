#!/usr/bin/env bash
# The project prefix owns activation; do not pick an unrelated system switch.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/rocq_toolchain_owner.sh"
: "${OPAMROOT:?Set the explicit project/image-owned OPAMROOT}"
[[ "$OPAMROOT" = /* && "$OPAMROOT" != / && "$#" -gt 0 ]] || { echo 'An absolute project prefix and command are required' >&2; exit 1; }
tool_bin="${OPAMROOT%/*}/pergyra-rocq-tools"
[[ -x "$tool_bin/opam" ]] || { echo 'Run scripts/install_rocq_toolchain.sh first; no system-opam fallback' >&2; exit 1; }
export PATH="$tool_bin:$PATH"
exec opam exec --switch="$PGY_ROCQ_SWITCH" -- "$@"

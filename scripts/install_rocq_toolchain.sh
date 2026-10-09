#!/usr/bin/env bash
# Installs only a named project switch, preserving other switches and Coq.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/rocq_toolchain_owner.sh"
: "${OPAMROOT:?Set an explicit project/image-owned OPAMROOT}"
[[ "$OPAMROOT" = /* ]] || { echo 'OPAMROOT must be absolute' >&2; exit 1; }
command -v opam >/dev/null 2>&1 || { echo 'opam is required' >&2; exit 1; }
# Ubuntu's opam 2.1 lacks the security fixes in >= 2.5.2. Keep the system
# executable intact, and use a checksum-pinned upstream binary in our prefix.
[[ "$OPAMROOT" != / && "${OPAMROOT%/*}" != '' ]] || { echo 'OPAMROOT must name a project prefix' >&2; exit 1; }
tool_bin="${OPAMROOT%/*}/pergyra-rocq-tools"
opam_version=2.6.1
opam_sha=b533d393a6967150d6ef41cb3d71e147e93ecbabeb59e921aa412c0ae79ebe3d
[[ "$(uname -s)/$(uname -m)" = Linux/x86_64 ]] || { echo 'Pinned opam bootstrap supports Linux/x86_64 only' >&2; exit 1; }
if [[ ! -f "$tool_bin/opam" ]] || ! printf '%s  %s\n' "$opam_sha" "$tool_bin/opam" | sha256sum -c --status; then
    command -v curl >/dev/null 2>&1 || { echo 'curl is required for the verified opam bootstrap' >&2; exit 1; }
    download="$(mktemp)"
    trap 'rm -f "$download"' EXIT
    curl --fail --location --proto '=https' --tlsv1.2 \
        "https://github.com/ocaml/opam/releases/download/$opam_version/opam-$opam_version-x86_64-linux" \
        --output "$download"
    printf '%s  %s\n' "$opam_sha" "$download" | sha256sum -c
    mkdir -p "$tool_bin"
    install -m 755 "$download" "$tool_bin/opam"
    rm -f "$download"
    trap - EXIT
fi
export PATH="$tool_bin:$PATH"
[[ "$(opam --version)" = "$opam_version" ]] || { echo 'Pinned opam version mismatch' >&2; exit 1; }
if [[ -n "${GITHUB_PATH:-}" ]]; then printf '%s\n' "$tool_bin" >> "$GITHUB_PATH"; fi
if [[ ! -f "$OPAMROOT/config" ]]; then
    opam init --bare --no-setup --disable-sandboxing -y
fi
if ! opam switch list --short | grep -Fxq "$PGY_ROCQ_SWITCH"; then
    # An OCaml executable inherited from another opam switch is not a system
    # compiler in this fresh root. Build the named compiler in our own prefix;
    # do not let image PATH or the newest ocaml-system package choose it.
    opam switch create "$PGY_ROCQ_SWITCH" \
        "ocaml-base-compiler=$PGY_ROCQ_BOOTSTRAP_OCAML_VERSION" \
        --jobs="${PGY_ROCQ_INSTALL_JOBS:-4}" -y
fi
opam update --switch="$PGY_ROCQ_SWITCH" -y
opam install --switch="$PGY_ROCQ_SWITCH" -y --jobs="${PGY_ROCQ_INSTALL_JOBS:-4}" \
    "rocq-core=$PGY_ROCQ_VERSION" "rocq-runtime=$PGY_ROCQ_VERSION" \
    "rocq-stdlib=$PGY_ROCQ_STDLIB_VERSION"
opam exec --switch="$PGY_ROCQ_SWITCH" -- bash -c \
    'source "$1/rocq_toolchain_owner.sh"; pgy_rocq_require || exit $?; opam list --installed rocq-core rocq-runtime rocq-stdlib' \
    bash "$SCRIPT_DIR"

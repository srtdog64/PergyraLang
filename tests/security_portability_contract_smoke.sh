#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[security-portability] $*" >&2
    exit 1
}

require_term() {
    local rel="$1"
    local term="$2"

    grep -Fq -- "$term" "$ROOT_DIR/$rel" ||
        fail "$rel missing term: $term"
}

require_term "Makefile" "check-security-toolchain:"
require_term "Makefile" "openssl/evp.h"
require_term "Makefile" "-lssl -lcrypto"
require_term "Makefile" "security test preflight requires OpenSSL development headers"

require_term "docs/03_security_mode_design.md" "not a completed third-party cryptographic audit"
require_term "docs/03_security_mode_design.md" "make check-security-toolchain"
require_term "docs/03_security_mode_design.md" "Platform claims must be tied to CI evidence"

require_term "TODO.md" "Security/runtime portability needs narrower claims"
require_term "TODO.md" "check-security-toolchain"

echo "[security-portability] security dependency and claim-scope contract is gated"

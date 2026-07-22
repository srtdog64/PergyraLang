#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

require() {
    local needle="$1"
    local file="$2"
    if ! grep -Fq "$needle" "$ROOT_DIR/$file"; then
        echo "[mir-lowering-api] missing '$needle' in $file" >&2
        exit 1
    fi
}

require "PGY_MIR_LOWER_PROTOCOL_ID" "src/compiler/mir.h"
require "PGY_MIR_LOWER_PROTOCOL_VERSION" "src/compiler/mir.h"
require "MIRLowerRequest" "src/compiler/mir.h"
require "mir_lower_request_init" "src/compiler/mir.h"
require "mir_lower_request_init" "src/compiler/mir.c"
require "strcmp(request->protocol_id, PGY_MIR_LOWER_PROTOCOL_ID)" \
    "src/compiler/mir.c"
require "request->protocol_version != PGY_MIR_LOWER_PROTOCOL_VERSION" \
    "src/compiler/mir.c"
require "unsupported protocol id/version" "src/compiler/mir.c"
require "mir_lower_request_init(&mir_request" "src/compiler/driver_app.c"
require "mir = mir_lower(&mir_request" "src/compiler/driver_app.c"

if rg -n --glob '*.c' --glob '*.h' --glob '*.cases.h' \
    'mir_lower\((hir|\*hir_out|\*\*hir_out),' \
    "$ROOT_DIR/src" >/dev/null; then
    echo "[mir-lowering-api] unversioned positional MIR lowering call remains" >&2
    exit 1
fi

echo "[mir-lowering-api] versioned request identity/version and native callers are wired"

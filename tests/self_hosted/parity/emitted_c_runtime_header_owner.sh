#!/usr/bin/env bash
# Owns the emitted-C runtime-header and compiler-profile contract.

pgy_selfhost_emitted_c_uses_runtime_headers() {
    local emitted_c="$1"
    grep -Eq '#include "pgy_runtime(_[^"]*)?\.h"' "$emitted_c"
}
pgy_selfhost_select_emitted_c_compile_profile() {
    local profile=${PGY_SELFHOST_CC_PROFILE:-release}
    if [[ $profile == release ]]; then
        PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS=(-O3 -fwrapv -fno-strict-aliasing)
        return 0
    fi
    if [[ $profile == test ]]; then
        PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS=(-O0 -fwrapv -fno-strict-aliasing)
        return 0
    fi
    echo self-host-emitted-c-profile-invalid
    return 2
}

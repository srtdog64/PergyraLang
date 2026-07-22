#!/usr/bin/env bash
# Owns classification of emitted C that consumes canonical runtime headers.

pgy_selfhost_emitted_c_uses_runtime_headers() {
    local emitted_c="$1"
    grep -Eq '#include "pgy_runtime(_[^"]*)?\.h"' "$emitted_c"
}

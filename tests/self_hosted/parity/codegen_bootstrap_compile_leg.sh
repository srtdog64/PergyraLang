#!/usr/bin/env bash
# Bounded emitted-C compiler leg shared by the bootstrap orchestrator.

compile_c_artifact_with_bounded_log() {
    local label="$1"
    local source="$2"
    local output="$3"
    local log="$B/${label}_cc.log"
    local tmp="$log.tmp"
    local limit="${PGY_SELFHOST_CC_LOG_LIMIT_BYTES:-65536}"
    local rc

    # Match the feature-test macros pgy itself passes when it compiles emitted C
    # (src/compiler/compiler.c). Without them, -std=c11 makes glibc hide POSIX
    # declarations the runtime headers use -- CLOCK_REALTIME in
    # pgy_runtime_panic_checked_inline.h is the one that bites -- so the gate
    # cannot build gen1 on Linux even though the compiler's own path builds the
    # same C fine. A bootstrap harness that compiles emitted C differently from
    # the compiler is not testing the same artifact.
    local -a cc_flags=(-std=c11)
    case "$(uname -s 2>/dev/null)" in
        Linux|*BSD|SunOS)
            cc_flags+=(-D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -pthread)
            ;;
        Darwin)
            cc_flags+=(-D_DARWIN_C_SOURCE -D_XOPEN_SOURCE=700 -pthread)
            ;;
    esac

    set +e
    "$CC" "${cc_flags[@]}" -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
        "$source" -o "$output" 2>&1 | awk -v limit="$limit" '
        BEGIN {
            written = 0
            truncated = 0
        }
        {
            line = $0 "\n"
            if (written < limit) {
                remaining = limit - written
                if (length(line) > remaining) {
                    printf "%s", substr(line, 1, remaining)
                    written = limit
                    truncated = 1
                } else {
                    printf "%s", line
                    written += length(line)
                }
            } else {
                truncated = 1
            }
        }
        END {
            if (truncated) {
                printf "\n[self-host-bootstrap] compiler log truncated at %s bytes\n", limit
            }
        }
    ' >"$tmp"
    rc=${PIPESTATUS[0]}
    set -e
    mv "$tmp" "$log"
    return "$rc"
}

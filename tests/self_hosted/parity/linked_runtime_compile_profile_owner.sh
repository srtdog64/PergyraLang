#!/usr/bin/env bash
# Runtime-TU feature visibility, matching Makefile's platform profile.
# Independent from emitted-program optimization and admitted language facts.
pgy_selfhost_select_linked_runtime_compile_profile() {
    local platform
    platform="$(uname -s)" || return 2
    PGY_SELFHOST_RUNTIME_C_COMPILE_FLAGS=(-std=c11 -O0 -DPGY_LLVM_ENABLED -pthread)
    case "$platform" in
        Linux*)
            PGY_SELFHOST_RUNTIME_C_COMPILE_FLAGS+=(
                -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE
            ) ;;
        Darwin) PGY_SELFHOST_RUNTIME_C_COMPILE_FLAGS+=(-D_DARWIN_C_SOURCE -D_XOPEN_SOURCE=700) ;;
        MINGW*|MSYS*|CYGWIN*) ;;
        *) echo "self-host-runtime-compile-platform-invalid: $platform" >&2; return 2 ;;
    esac
}

#!/usr/bin/env bash
# Rewrite a linker response file into paths the toolchain can actually open.
#
# MSYS converts POSIX paths in command-line ARGUMENTS before a native Windows
# tool sees them, but it cannot reach inside an @response file. So a link whose
# BUILD_DIR is an absolute MSYS path (`/d/repo/...`) writes object paths that
# `ld.exe` reports as "cannot find" even though every object is on disk. The
# ordinary build never hits this because its BUILD_DIR is relative; passing an
# absolute BUILD_DIR, as the ci-windows cross-toolchain branch does, is what
# exposes it.
#
# No-op unless cygpath is present and the file actually contains a leading-slash
# path, so this is inert on Linux, macOS, and MSYS-native toolchains.

set -euo pipefail

rsp="${1:?usage: $0 <response-file>}"
[ -f "$rsp" ] || exit 0
command -v cygpath >/dev/null 2>&1 || exit 0
grep -Eq '(^| )/' "$rsp" 2>/dev/null || exit 0

converted="$(
    tr ' ' '\n' < "$rsp" |
    while IFS= read -r token; do
        [ -n "$token" ] || continue
        case "$token" in
            /*) cygpath -m "$token" ;;
            *)  printf '%s\n' "$token" ;;
        esac
    done
)"

printf '%s\n' "$converted" > "$rsp"

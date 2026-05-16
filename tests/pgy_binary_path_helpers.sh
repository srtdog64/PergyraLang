#!/usr/bin/env bash

pgy_binary_expects_windows_paths() {
    local bin="$1"
    if [[ ! -x "$bin" ]]; then
        return 1
    fi
    if [[ "$bin" != *.exe ]]; then
        return 1
    fi
    if command -v file >/dev/null 2>&1 \
        && file "$bin" 2>/dev/null | grep -Eq "ELF|Mach-O"; then
        return 1
    fi
    return 0
}

pgy_path_for_compiler() {
    local pgy="$1"
    local path="$2"

    if ! pgy_binary_expects_windows_paths "$pgy"; then
        printf '%s\n' "$path"
        return 0
    fi
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$path"
        return 0
    fi
    if command -v wslpath >/dev/null 2>&1; then
        wslpath -w "$path"
        return 0
    fi
    if [[ "$path" =~ ^/mnt/([A-Za-z])/(.*)$ ]]; then
        local drive="${BASH_REMATCH[1]}"
        local rest="${BASH_REMATCH[2]//\//\\}"
        printf '%s:\\%s\n' "${drive^^}" "$rest"
        return 0
    fi
    if [[ "$path" =~ ^/([A-Za-z])/(.*)$ ]]; then
        local drive="${BASH_REMATCH[1]}"
        local rest="${BASH_REMATCH[2]//\//\\}"
        printf '%s:\\%s\n' "${drive^^}" "$rest"
        return 0
    fi
    printf '%s\n' "$path"
}

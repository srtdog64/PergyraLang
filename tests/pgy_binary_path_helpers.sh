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

pgy_prepend_windows_runtime_paths() {
    case "$(uname -s 2>/dev/null || echo unknown)" in
        MINGW*|MSYS*|CYGWIN*)
            # This intentionally prepends even if PATH already contains a
            # candidate. Launch stability depends on the final priority order,
            # not merely on directory presence.
            if [[ -n "${MSYSTEM_PREFIX:-}" ]]; then
                if [[ -d "${MSYSTEM_PREFIX}/bin" ]]; then
                    PATH="${MSYSTEM_PREFIX}/bin:$PATH"
                fi
            fi
            for candidate in \
                "/ucrt64/bin" \
                "/clang64/bin" \
                "/mingw64/bin" \
                "/c/LLVM/bin" \
                "/c/Program Files/LLVM/bin" \
                "/c/msys64/mingw64/bin" \
                "/c/ProgramData/mingw64/mingw64/bin"; do
                [[ -d "$candidate" ]] || continue
                PATH="$candidate:$PATH"
            done
            if [[ -n "${LLVM_INSTALL:-}" ]]; then
                if [[ -d "$LLVM_INSTALL" ]]; then
                    PATH="$LLVM_INSTALL:$PATH"
                fi
                if [[ -d "$LLVM_INSTALL/bin" ]]; then
                    PATH="$LLVM_INSTALL/bin:$PATH"
                fi
            fi
            ;;
    esac
}

pgy_windows_powershell_path_prefix() {
    local prefix=""
    local candidate
    local win_candidate

    if [[ -n "${MSYSTEM_PREFIX:-}" && -d "${MSYSTEM_PREFIX}/bin" ]]; then
        win_candidate="$(pgy_path_for_windows_tool "${MSYSTEM_PREFIX}/bin")"
        prefix="${prefix}${win_candidate};"
    fi

    for candidate in \
        "/c/LLVM/bin" \
        "/c/Program Files/LLVM/bin" \
        "/c/msys64/mingw64/bin" \
        "/c/ProgramData/mingw64/mingw64/bin"; do
        [[ -d "$candidate" ]] || continue
        win_candidate="$(pgy_path_for_windows_tool "$candidate")"
        prefix="${prefix}${win_candidate};"
    done

    if [[ -n "${LLVM_INSTALL:-}" ]]; then
        if [[ -d "$LLVM_INSTALL" ]]; then
            win_candidate="$(pgy_path_for_windows_tool "$LLVM_INSTALL")"
            prefix="${prefix}${win_candidate};"
        fi
        if [[ -d "$LLVM_INSTALL/bin" ]]; then
            win_candidate="$(pgy_path_for_windows_tool "$LLVM_INSTALL/bin")"
            prefix="${prefix}${win_candidate};"
        fi
    fi

    printf '%s' "${prefix}C:\\Program Files\\LLVM\\bin;C:\\ProgramData\\mingw64\\mingw64\\bin;C:\\msys64\\mingw64\\bin;"
}

pgy_path_for_windows_tool() {
    local path="$1"

    if command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$path"
        return 0
    fi
    if command -v wslpath >/dev/null 2>&1; then
        wslpath -w "$path"
        return 0
    fi
    if [[ "$path" =~ ^/mnt/([A-Za-z])/(.*)$ ]]; then
        local drive
        local rest="${BASH_REMATCH[2]//\//\\}"
        drive="$(printf '%s' "${BASH_REMATCH[1]}" | tr '[:lower:]' '[:upper:]')"
        printf '%s:\\%s\n' "$drive" "$rest"
        return 0
    fi
    if [[ "$path" =~ ^/([A-Za-z])/(.*)$ ]]; then
        local drive
        local rest="${BASH_REMATCH[2]//\//\\}"
        drive="$(printf '%s' "${BASH_REMATCH[1]}" | tr '[:lower:]' '[:upper:]')"
        printf '%s:\\%s\n' "$drive" "$rest"
        return 0
    fi
    printf '%s\n' "$path"
}

pgy_path_for_bash_tool() {
    local path="$1"

    if command -v cygpath >/dev/null 2>&1; then
        cygpath -u "$path" 2>/dev/null || printf '%s\n' "$path"
        return 0
    fi
    if [[ "$path" =~ ^([A-Za-z]):[\\/](.*)$ ]]; then
        local drive
        local rest="${BASH_REMATCH[2]//\\//}"
        drive="$(printf '%s' "${BASH_REMATCH[1]}" | tr '[:upper:]' '[:lower:]')"
        printf '/%s/%s\n' "$drive" "$rest"
        return 0
    fi
    printf '%s\n' "$path"
}

pgy_path_for_compiler() {
    local pgy="$1"
    local path="$2"

    if ! pgy_binary_expects_windows_paths "$pgy"; then
        printf '%s\n' "$path"
        return 0
    fi
    pgy_path_for_windows_tool "$path"
}

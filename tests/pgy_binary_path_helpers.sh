#!/usr/bin/env bash

pgy_binary_magic_kind() {
    local bin="$1"
    local magic=""

    if command -v od >/dev/null 2>&1; then
        magic="$(od -An -tx1 -N4 "$bin" 2>/dev/null | tr -d ' \n' || true)"
    fi

    case "$magic" in
        4d5a*) printf '%s\n' "PE" ;;
        7f454c46) printf '%s\n' "ELF" ;;
        feedface|cefaedfe|feedfacf|cffaedfe|cafebabe|bebafeca) printf '%s\n' "Mach-O" ;;
    esac
}

pgy_binary_expects_windows_paths() {
    local bin="$1"
    local magic=""
    if [[ ! -x "$bin" ]]; then
        return 1
    fi
    if [[ "$bin" != *.exe ]]; then
        return 1
    fi
    magic="$(pgy_binary_magic_kind "$bin")"
    if [[ "$magic" == "PE" ]]; then
        return 0
    fi
    if [[ "$magic" == "ELF" || "$magic" == "Mach-O" ]]; then
        return 1
    fi
    if command -v file >/dev/null 2>&1 \
        && file "$bin" 2>/dev/null | grep -Eq "ELF|Mach-O"; then
        return 1
    fi
    return 0
}

pgy_binary_is_runnable_here() {
    local bin="$1"
    local desc=""
    local magic=""

    if [[ ! -x "$bin" ]]; then
        return 1
    fi

    magic="$(pgy_binary_magic_kind "$bin")"
    if command -v file >/dev/null 2>&1; then
        desc="$(file "$bin" 2>/dev/null || true)"
    fi

    case "$(uname -s 2>/dev/null || echo unknown)" in
        MINGW*|MSYS*|CYGWIN*)
            if [[ "$magic" == "PE" ]]; then
                return 0
            fi
            if [[ "$magic" == "ELF" || "$magic" == "Mach-O" ]]; then
                return 1
            fi
            if [[ -n "$desc" ]]; then
                if printf '%s\n' "$desc" | grep -Eq "PE32|MS Windows"; then
                    return 0
                fi
                if printf '%s\n' "$desc" | grep -Eq "ELF|Mach-O"; then
                    return 1
                fi
            fi
            [[ "$bin" == *.exe ]]
            ;;
        Linux*)
            if [[ "$magic" == "ELF" ]]; then
                return 0
            fi
            if [[ "$magic" == "PE" || "$magic" == "Mach-O" ]]; then
                return 1
            fi
            if [[ -n "$desc" ]]; then
                if printf '%s\n' "$desc" | grep -Eq "ELF"; then
                    return 0
                fi
                if printf '%s\n' "$desc" | grep -Eq "PE32|MS Windows|Mach-O"; then
                    return 1
                fi
            fi
            return 0
            ;;
        Darwin*)
            if [[ "$magic" == "Mach-O" ]]; then
                return 0
            fi
            if [[ "$magic" == "PE" || "$magic" == "ELF" ]]; then
                return 1
            fi
            if [[ -n "$desc" ]]; then
                if printf '%s\n' "$desc" | grep -Eq "Mach-O"; then
                    return 0
                fi
                if printf '%s\n' "$desc" | grep -Eq "PE32|MS Windows|ELF"; then
                    return 1
                fi
            fi
            return 0
            ;;
        *)
            return 0
            ;;
    esac
}

pgy_select_optional_exe_binary() {
    local bin="$1"
    if [[ "$bin" != *.exe && -x "${bin}.exe" ]]; then
        printf '%s\n' "${bin}.exe"
        return 0
    fi
    printf '%s\n' "$bin"
}

pgy_require_runnable_binary_here() {
    local label="$1"
    local bin="$2"

    if pgy_binary_is_runnable_here "$bin"; then
        return 0
    fi

    echo "[$label] binary is not runnable on this host: $bin" >&2
    if command -v file >/dev/null 2>&1; then
        file "$bin" >&2 || true
    fi
    return 1
}

pgy_windows_path_is_git_runtime() {
    case "$1" in
        *"\\Git\\mingw64\\bin"|*"\\Git\\usr\\bin")
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

pgy_prepend_windows_runtime_paths() {
    case "$(uname -s 2>/dev/null || echo unknown)" in
        MINGW*|MSYS*|CYGWIN*)
            # This intentionally prepends even if PATH already contains a
            # candidate. Launch stability depends on the final priority order,
            # not merely on directory presence.
            for candidate in \
                "/c/LLVM/bin" \
                "/c/Program Files/LLVM/bin" \
                "/c/msys64/ucrt64/bin" \
                "/c/msys64/clang64/bin" \
                "/c/msys64/mingw64/bin" \
                "/ucrt64/bin" \
                "/clang64/bin" \
                "/mingw64/bin" \
                "/c/ProgramData/mingw64/mingw64/bin"; do
                [[ -d "$candidate" ]] || continue
                if pgy_windows_path_is_git_runtime "$(pgy_path_for_windows_tool "$candidate")"; then
                    continue
                fi
                PATH="$candidate:$PATH"
            done
            if [[ -n "${MSYSTEM_PREFIX:-}" && -d "${MSYSTEM_PREFIX}/bin" ]]; then
                local msystem_candidate
                msystem_candidate="$(pgy_path_for_windows_tool "${MSYSTEM_PREFIX}/bin")"
                if ! pgy_windows_path_is_git_runtime "$msystem_candidate"; then
                    PATH="${MSYSTEM_PREFIX}/bin:$PATH"
                fi
            fi
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

    for candidate in \
        "/c/LLVM/bin" \
        "/c/Program Files/LLVM/bin" \
        "/c/msys64/ucrt64/bin" \
        "/c/msys64/clang64/bin" \
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

    if [[ -n "${MSYSTEM_PREFIX:-}" && -d "${MSYSTEM_PREFIX}/bin" ]]; then
        win_candidate="$(pgy_path_for_windows_tool "${MSYSTEM_PREFIX}/bin")"
        if ! pgy_windows_path_is_git_runtime "$win_candidate"; then
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

pgy_powershell_quote() {
    local value="${1//\'/\'\'}"
    printf "'%s'" "$value"
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

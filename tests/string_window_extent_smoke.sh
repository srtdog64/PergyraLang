#!/usr/bin/env bash
# String-window extent admission gate
# (docs/agent_work_directives/string_window_extent_closure_2026-10-10.md).
#
# Both routes judge every fixture. `native` runs the C pipeline
# (--native-pipeline): each accept_* fixture must compile and print exactly
# its .expected text, and each reject_* fixture must be refused with the extent
# diagnostic and leave no executable. `selfhost` runs the installed Pergyra
# driver up to its verified source-to-MIR boundary, where semantic admission
# ends: each accept_* fixture must yield a MIR document, and each reject_*
# fixture must be refused with `string_window_extent_unproven` and leave none.
# The self-host backends do not lower every window builtin, so execution is the
# native route's evidence. A route whose compiler is missing fails; it is never
# skipped.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
if [[ "$DRIVER" != *.exe ]] && pgy_binary_expects_windows_paths "${DRIVER}.exe"; then
    DRIVER="${DRIVER}.exe"
fi
ROUTES="${PGY_STRING_WINDOW_EXTENT_ROUTES:-native selfhost}"
CASES="$ROOT_DIR/tests/cases/string_window_extent"
WORK="$ROOT_DIR/.tmp/string_window_extent"
mkdir -p "$WORK"

fail() {
    echo "[string-window-extent] FAIL: $*" >&2
    exit 1
}

# Compile one fixture on one route; the exit status is the admission verdict.
admit() {
    local route="$1" source="$2" artifact="$3" log="$4"
    case "$route" in
        native)
            (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$source")" \
                --native-pipeline --error-format=json \
                -o "$(pgy_path_for_compiler "$PGY" "$artifact")") >"$log" 2>&1
            ;;
        selfhost)
            # The driver takes repository-relative paths, as its other gates do.
            (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
                "${source#"$ROOT_DIR"/}" -o "${artifact#"$ROOT_DIR"/}") >"$log" 2>&1
            ;;
        *) fail "unknown route: $route" ;;
    esac
}

route_code() {
    case "$1" in
        native) echo "PGY_SEM_STRING_WINDOW_EXTENT_UNPROVEN" ;;
        selfhost) echo "string_window_extent_unproven" ;;
    esac
}

accepted=0
refused=0
for route in $ROUTES; do
    case "$route" in
        native) [[ -x "$PGY" ]] || fail "missing compiler binary: $PGY" ;;
        selfhost) [[ -x "$DRIVER" ]] || fail "missing self-host driver: $DRIVER" ;;
        *) fail "unknown route: $route" ;;
    esac
    code="$(route_code "$route")"
    for source in "$CASES"/accept_*.pgy "$CASES"/reject_*.pgy; do
        name="$(basename "$source" .pgy)"
        artifact="$WORK/${route}_${name}.exe"
        [[ "$route" == selfhost ]] && artifact="$WORK/${route}_${name}.mir.json"
        log="$WORK/${route}_${name}.log"
        rm -f "$artifact"
        if admit "$route" "$source" "$artifact" "$log"; then
            compiled=1
        else
            compiled=0
        fi
        case "$name" in
            accept_*)
                [[ "$compiled" -eq 1 ]] || { cat "$log" >&2; fail "$route refused $name"; }
                [[ -s "$artifact" ]] || fail "$route admitted $name without an artifact"
                if [[ "$route" == native ]]; then
                    expected="$(cat "$CASES/$name.expected")"
                    observed="$("$artifact" 2>/dev/null | tr -d '\r')"
                    [[ "$observed" == "$expected" ]] ||
                        fail "$route $name printed '$observed', expected '$expected'"
                fi
                accepted=$((accepted + 1))
                ;;
            reject_*)
                [[ "$compiled" -eq 0 ]] || fail "$route accepted $name"
                [[ ! -e "$artifact" ]] || fail "$route left an artifact for $name"
                count="$(grep -o "$code" "$log" | wc -l | tr -d ' ')"
                [[ "$count" -ge 1 ]] || { cat "$log" >&2; fail "$route refused $name without $code"; }
                refused=$((refused + 1))
                ;;
        esac
    done
    echo "[string-window-extent] route=$route ok"
done
echo "[string-window-extent] PASS ($accepted accepted, $refused refused; routes: $ROUTES)"

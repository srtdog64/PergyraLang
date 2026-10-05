#!/usr/bin/env bash
# Value/lifetime checks plus source admission; never emit/run the unsafe input.
set -Eeuo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
LABEL=generic-instance-owned-actuals
pgy_require_runnable_binary_here "$LABEL" "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
B="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/generic-owned-actuals.XXXXXX")"
fail() { echo "[$LABEL] $*; evidence=$B" >&2; exit 1; }
cd "$ROOT_DIR"
ALGORITHM=tests/self_hosted/parity/fixture/generic_instance_closure_probe.pgy
COPY=tests/self_hosted/parity/fixture/generic_instance_owned_actuals_probe.pgy
UNSAFE=tests/self_hosted/parity/fixture/generic_tuple_borrowed_scalar_store_negative.pgy
PROBE=tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy
sha256sum "$PGY" "$ALGORITHM" "$COPY" "$UNSAFE" "$PROBE" >"$B/input.sha256"
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z | xargs -0 sha256sum >"$B/import.sha256"
printf '%s\n' 'PASS: ordered tuples, forwarding, same-instance recursion' \
    'PASS: constant recursive fixed point' 'PASS: constructor-growing recursion refused' \
    'PASS: missing, malformed and crossed identities refused' \
    'PASS: declaration epoch invariance' >"$B/algorithm-expected"
printf 'GENERIC OWNED ACTUALS PASS\n' >"$B/copy-expected"
for backend in c llvm; do
    for target in algorithm copy; do
        input="$ALGORITHM"; [[ "$target" != copy ]] || input="$COPY"
        timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" "$input" \
            -o "$B/$backend-$target.exe" >"$B/$backend-$target.compile" 2>&1 || fail "$backend $target build refused"
        timeout 30 "$B/$backend-$target.exe" >"$B/$backend-$target.raw" 2>"$B/$backend-$target.err" || fail "$backend $target execution failed"
        test ! -s "$B/$backend-$target.err" || fail "$backend $target wrote stderr"
        tr -d '\r' <"$B/$backend-$target.raw" >"$B/$backend-$target.run"
        cmp "$B/$target-expected" "$B/$backend-$target.run" || fail "$backend $target value/lifetime oracle drift"
    done
    timeout 120 "$PGY" --native-pipeline --opt=dev "--backend=$backend" "$PROBE" \
        -o "$B/$backend-observer.exe" >"$B/$backend-observer.compile" 2>&1 || fail "$backend observer build refused"
    for target in algorithm copy unsafe; do
        input="$ALGORITHM"; [[ "$target" != copy ]] || input="$COPY"; [[ "$target" != unsafe ]] || input="$UNSAFE"
        timeout 60 "$B/$backend-observer.exe" "$input" diagnostic >"$B/$backend-$target.observe" 2>&1 || fail "$backend $target source observation failed"
        tr -d '\r' <"$B/$backend-$target.observe" >"$B/$backend-$target.normalized"
        if [[ "$target" == unsafe || "$target" == copy ]]; then
            grep -Fxq 'body_ok=false' "$B/$backend-$target.normalized" || fail "$backend retained borrowed tuple text"
            grep -Fxq 'body_diagnostic=borrow_boundary_escape' "$B/$backend-$target.normalized" || fail "$backend lost borrowed-text diagnostic"
            boundary=unproved_formal_element_use_entry
            [[ "$target" != copy ]] || boundary=owned_string_drop
            grep -Fq "boundary: $boundary" "$B/$backend-$target.normalized" || fail "$backend lost exact $boundary boundary"
        else
            grep -Fxq 'body_ok=true' "$B/$backend-$target.normalized" || fail "$backend refused source $target"
        fi
    done
done
sha256sum --quiet -c "$B/input.sha256"
sha256sum --quiet -c "$B/import.sha256"
sha256sum "$B"/*.exe >"$B/binaries.sha256"
echo "[$LABEL] native C/LLVM tuple/cleanup values; source owner admission, raw-retention and unproved-returned-array drop refusals PASS; evidence=$B"

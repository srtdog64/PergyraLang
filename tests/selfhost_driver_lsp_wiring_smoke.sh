#!/usr/bin/env bash
#
# selfhost_driver_lsp_wiring_smoke.sh — docs/150's rung ladder is a CONTRACT.
# A rung marked `landed` must name an artifact and a gate that exist on disk;
# a rung marked `blocked` must name both plus an explicit blocker; a rung
# marked `planned` must claim neither. This blocks fake self-host
# progress in either direction: claiming what does not exist, and building
# what the ladder never registered.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DOC="$ROOT_DIR/docs/150_selfhost_driver_lsp_wiring.md"

fail() { echo "[driver-lsp-wiring] FAIL: $*" >&2; exit 1; }

[ -f "$DOC" ] || fail "missing docs/150_selfhost_driver_lsp_wiring.md"

rows="$(sed -n '/DRIVER-LSP-RUNG-BEGIN/,/DRIVER-LSP-RUNG-END/p' "$DOC" \
    | grep -E '^\| (driver|lsp) \|')"
[ -n "$rows" ] || fail "docs/150 rung block has no rows"

expected_rungs="DRV-0 DRV-1 DRV-2 DRV-3 LSP-0 LSP-1 LSP-2a LSP-2b LSP-2c LSP-2d LSP-2e LSP-2f LSP-2g LSP-2h LSP-2i LSP-2 LSP-3"
for rung in $expected_rungs; do
    printf '%s\n' "$rows" | grep -Fq "| $rung |" ||
        fail "rung table lost row '$rung' (ladder rows may change status, not vanish)"
done

while IFS='|' read -r _ track rung status artifact gate _; do
    track="$(echo "$track" | tr -d ' ')"
    rung="$(echo "$rung" | tr -d ' ')"
    status="$(echo "$status" | tr -d ' ')"
    artifact="$(echo "$artifact" | tr -d ' ')"
    gate="$(echo "$gate" | tr -d ' ')"
    case "$track" in
        driver|lsp) ;;
        *) fail "$rung: unknown track '$track'" ;;
    esac
    case "$status" in
        landed)
            [ "$artifact" != "-" ] || fail "$rung is landed but names no artifact"
            [ "$gate" != "-" ] || fail "$rung is landed but names no gate"
            [ -e "$ROOT_DIR/$artifact" ] ||
                fail "$rung: landed artifact '$artifact' does not exist"
            [ -e "$ROOT_DIR/$gate" ] ||
                fail "$rung: landed gate '$gate' does not exist"
            ;;
        blocked)
            [ "$artifact" != "-" ] || fail "$rung is blocked but names no artifact"
            [ "$gate" != "-" ] || fail "$rung is blocked but names no gate"
            [ -e "$ROOT_DIR/$artifact" ] ||
                fail "$rung: blocked artifact '$artifact' does not exist"
            [ -e "$ROOT_DIR/$gate" ] ||
                fail "$rung: blocked gate '$gate' does not exist"
            grep -Fq "$rung blocker" "$DOC" ||
                fail "$rung is blocked but its blocker is not documented"
            ;;
        planned)
            [ "$artifact" = "-" ] && [ "$gate" = "-" ] ||
                fail "$rung is planned but claims artifact/gate (land it or clear the claim)"
            ;;
        *) fail "$rung: unknown status '$status'" ;;
    esac
done < <(printf '%s\n' "$rows")

# The gap register must stay visible until its rungs land: G-EXEC blocks
# DRV-3 and G-LSP-STREAM blocks the full LSP-2 session loop. G-STDIN is a
# landed prerequisite now consumed by LSP-2a.
printf '%s\n' "$rows" | grep -Fq \
    "| driver | DRV-2 | landed | src/self_hosted/compiler/driver_rung2_main.pgy | tests/self_hosted/parity/driver_rung2_body_parity.sh |" ||
    fail "DRV-2 must name the landed artifact-body semantic owner and parity gate"
grep -Fq '| **G-EXEC** ' "$DOC" ||
    fail "DRV-3 is planned but the G-EXEC gap entry vanished"
grep -Fq '| DRV-3 |' "$DOC" ||
    fail "G-EXEC must block DRV-3, not the landed DRV-2"
if grep -E '^\| \*\*G-EXEC\*\* .*\| DRV-2 \|$' "$DOC" >/dev/null; then
    fail "G-EXEC must not be registered against landed DRV-2"
fi
grep -Fq 'artifact-body semantic source-to-C (landed)' "$DOC" ||
    fail "DRV-2 responsibility text drifted from artifact-body semantic ownership"
grep -Fq 'self-host-driver-rung2-body-parity-test-smoke' "$ROOT_DIR/Makefile" ||
    fail "DRV-2 landed gate is not wired into Makefile"
grep -Fq 'self-host-live-replacement-test-smoke' "$ROOT_DIR/Makefile" ||
    fail "DRV-2 live replacement gate is not wired into Makefile"
grep -Fq 'pgy --self-driver <source.pgy>' "$DOC" ||
    fail "DRV-2 live replacement CLI is not documented"
grep -Fq '`--mir-json <file>`' "$DOC" ||
    fail "DRV-2 producer-first MIR CLI is not documented"
grep -Fq '`--emit-mir-json-verified`' "$DOC" ||
    fail "DRV-2 self MIR producer CLI is not documented"
grep -Fq '`--canonicalize-mir-json`' "$DOC" ||
    fail "DRV-2 canonical MIR parity CLI is not documented"
grep -Fq 'mir_fixtures=${#mir_fixture_rows[@]}' \
    "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_body_parity.sh" ||
    fail "DRV-2 MIR integration gate is not wired into the landed parity runner"
if printf '%s\n' "$rows" | grep -Fq "| LSP-2 | planned |"; then
    case "$(cat "$DOC")" in
        *G-LSP-STREAM*) ;;
        *) fail "LSP-2 is planned but the G-LSP-STREAM gap entry vanished" ;;
    esac
fi

if grep -Fq "tests/read_stdin_builtin_smoke.sh" "$DOC"; then
    [ -e "$ROOT_DIR/tests/read_stdin_builtin_smoke.sh" ] ||
        fail "G-STDIN claims read-stdin substrate smoke, but the script is missing"
    grep -Fq "read-stdin-builtin-test-smoke" "$ROOT_DIR/Makefile" ||
        fail "G-STDIN claims read-stdin substrate smoke, but the Makefile target is missing"
fi

if grep -Fq "tests/self_hosted/parity/lsp_transport_frame_parity.sh" "$DOC"; then
    [ -e "$ROOT_DIR/tests/self_hosted/parity/lsp_transport_frame_parity.sh" ] ||
        fail "LSP-2a claims transport-frame parity, but the script is missing"
    grep -Fq "self-host-lsp-transport-frame-parity-test-smoke" "$ROOT_DIR/Makefile" ||
        fail "LSP-2a claims transport-frame parity, but the Makefile target is missing"
fi

if grep -Fq "tests/self_hosted/parity/lsp_transport_stream_parity.sh" "$DOC"; then
    [ -e "$ROOT_DIR/tests/self_hosted/parity/lsp_transport_stream_parity.sh" ] ||
        fail "LSP-2b claims transport-stream parity, but the script is missing"
    grep -Fq "self-host-lsp-transport-stream-parity-test-smoke" "$ROOT_DIR/Makefile" ||
        fail "LSP-2b claims transport-stream parity, but the Makefile target is missing"
fi

if grep -Fq "tests/self_hosted/parity/lsp_request_dispatch_parity.sh" "$DOC"; then
    [ -e "$ROOT_DIR/tests/self_hosted/parity/lsp_request_dispatch_parity.sh" ] ||
        fail "LSP-2c claims request-dispatch parity, but the script is missing"
    grep -Fq "self-host-lsp-request-dispatch-parity-test-smoke" "$ROOT_DIR/Makefile" ||
        fail "LSP-2c claims request-dispatch parity, but the Makefile target is missing"
fi

if grep -Fq "tests/self_hosted/parity/lsp_response_emission_parity.sh" "$DOC"; then
    [ -e "$ROOT_DIR/tests/self_hosted/parity/lsp_response_emission_parity.sh" ] ||
        fail "LSP-2d claims response-emission parity, but the script is missing"
    grep -Fq "self-host-lsp-response-emission-parity-test-smoke" "$ROOT_DIR/Makefile" ||
        fail "LSP-2d claims response-emission parity, but the Makefile target is missing"
fi

if grep -Fq "tests/self_hosted/parity/lsp_session_replay_parity.sh" "$DOC"; then
    [ -e "$ROOT_DIR/tests/self_hosted/parity/lsp_session_replay_parity.sh" ] ||
        fail "LSP-2e claims session-replay parity, but the script is missing"
    grep -Fq "self-host-lsp-session-replay-parity-test-smoke" "$ROOT_DIR/Makefile" ||
        fail "LSP-2e claims session-replay parity, but the Makefile target is missing"
fi

if grep -Fq "tests/self_hosted/parity/lsp_document_store_parity.sh" "$DOC"; then
    [ -e "$ROOT_DIR/tests/self_hosted/parity/lsp_document_store_parity.sh" ] ||
        fail "LSP-2f claims document-store parity, but the script is missing"
    grep -Fq "self-host-lsp-document-store-parity-test-smoke" "$ROOT_DIR/Makefile" ||
        fail "LSP-2f claims document-store parity, but the Makefile target is missing"
fi

if grep -Fq "tests/self_hosted/parity/lsp_session_state_parity.sh" "$DOC"; then
    [ -e "$ROOT_DIR/tests/self_hosted/parity/lsp_session_state_parity.sh" ] ||
        fail "LSP-2h claims session-state parity, but the script is missing"
    grep -Fq "self-host-lsp-session-state-parity-test-smoke" "$ROOT_DIR/Makefile" ||
        fail "LSP-2h claims session-state parity, but the Makefile target is missing"
fi

if grep -Fq "tests/self_hosted/parity/lsp_hover_content_parity.sh" "$DOC"; then
    [ -e "$ROOT_DIR/tests/self_hosted/parity/lsp_hover_content_parity.sh" ] ||
        fail "LSP-2i claims hover-content parity, but the script is missing"
    grep -Fq "self-host-lsp-hover-content-parity-test-smoke" "$ROOT_DIR/Makefile" ||
        fail "LSP-2i claims hover-content parity, but the Makefile target is missing"
fi

echo "[driver-lsp-wiring] rung ladder honest (landed==exists, blocked==documented, planned==unclaimed, gaps visible)"

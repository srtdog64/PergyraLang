#!/usr/bin/env bash
# Bootstrap corpus gate for the self-hosted policy owners.
#
# The Stage-5 terminus is a compiler that eats its own source (docs/self_hosted/
# 01_staged_roadmap.md). The policy owners under src/self_hosted/{parallel,
# compiler} ARE self-host source -- so each one is either inside the integrated
# driver's bounded surface today, or it is a named piece of the corpus the
# bootstrap cannot yet eat. Both facts are worth pinning; neither should drift
# silently.
#
# For every policy manifest this gate runs the SAME source through the
# Pergyra-built integrated driver (driver_seed.exe, built by the driver
# bootstrap from the self-parser + self-codegen) and through the native-built
# oracle driver (driver_oracle.exe), then checks the declared status:
#
#   in_subset      both drivers emit C, the two emissions are byte-identical
#                  after normalization, the self-built C compiles, runs, and
#                  reproduces the SAME golden the native gates pin. Regressing
#                  out of the subset fails.
#   out_of_subset  the driver refuses (CODEGEN ERROR / nonzero) -- the honest
#                  record of what the bootstrap cannot eat yet. If it starts
#                  succeeding, the gate fails until the row is promoted, so
#                  corpus growth is an explicit edit, never an accident.
#
# This mirrors the reachability contract's live/declared_only asymmetry, on the
# bootstrap axis: coverage can neither rot nor grow unrecorded.
#
# Prereq: the driver bootstrap has produced its artifacts in
# .tmp/self_hosted/driver/bootstrap (the Makefile target depends on it). The
# seeds are validated as runnable-on-this-host first: a stale cross-platform
# .tmp (WSL<->Windows) must fail with "reseed", not with format noise.
#
# Usage: PGY_BIN=bin/pgy.exe bash tests/selfhost_bootstrap_policy_corpus_smoke.sh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="selfhost-bootstrap-policy-corpus"
# Overridable so an isolated run (or a probe against a locally built driver
# pair) does not have to share .tmp with a concurrent bootstrap session.
DRIVER_BUILD="${PGY_SELFHOST_DRIVER_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/bootstrap}"
SEED_BIN="$DRIVER_BUILD/driver_seed.exe"
ORACLE_BIN="$DRIVER_BUILD/driver_oracle.exe"
BUILD_DIR="${PGY_SELFHOST_POLICY_CORPUS_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/policy_corpus}"
mkdir -p "$BUILD_DIR"

CC="${PGY_SELFHOST_CC:-gcc}"

fail() { echo "[$LABEL] $*" >&2; exit 1; }

for bin in "$SEED_BIN" "$ORACLE_BIN"; do
    [[ -f "$bin" ]] || fail "missing driver artifact: $bin (run make self-host-driver-bootstrap-test-smoke first)"
    pgy_binary_is_runnable_here "$bin" || fail "driver artifact is not runnable on this host: $bin -- stale cross-platform .tmp; reseed with make self-host-driver-bootstrap-test-smoke"
done
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"

# ---- the corpus table -------------------------------------------------------
# name | manifest source | native golden | declared status
# Statuses are EMPIRICAL pins, not aspirations. Promote a row to in_subset in
# the same change that makes the driver actually eat it.
CORPUS=(
    "chunk_policy|src/self_hosted/parallel/chunk_policy_manifest.pgy|src/self_hosted/parallel/expected_chunk_policy_manifest.txt|out_of_subset"
    "lane_policy|src/self_hosted/parallel/lane_policy_manifest.pgy|src/self_hosted/parallel/expected_lane_policy_manifest.txt|out_of_subset"
    "reachability|src/self_hosted/compiler/reachability_manifest.pgy|src/self_hosted/compiler/expected_reachability_manifest.txt|out_of_subset"
)

emit_via() { # $1=label $2=bin $3=source_rel $4=out_c ; rc 0 = emitted clean C
    local label="$1" bin="$2" src_rel="$3" out_c="$4"
    rm -f "$out_c" "$BUILD_DIR/$label.out" "$BUILD_DIR/$label.err"
    if ! (cd "$ROOT_DIR" && "$bin" "$src_rel" \
        "$(pgy_selfhost_path_relative_to_root "$out_c")" \
        >"$BUILD_DIR/$label.out" 2>"$BUILD_DIR/$label.err"); then
        return 1
    fi
    [[ -s "$out_c" ]] || return 1
    if grep -q '^CODEGEN ERROR' "$out_c"; then
        return 1
    fi
    return 0
}

controlled_refusal() { # $1=corpus row name
    local name="$1"
    grep -Fq "CODEGEN ERROR" \
        "$BUILD_DIR/${name}_self.err" \
        "$BUILD_DIR/${name}_self.out" \
        "$BUILD_DIR/${name}_self.c" 2>/dev/null
}

rows=0
in_rows=0
out_rows=0
for row in "${CORPUS[@]}"; do
    IFS='|' read -r name src golden status <<<"$row"
    [[ -f "$ROOT_DIR/$src" ]] || fail "corpus source missing: $src"
    [[ -f "$ROOT_DIR/$golden" ]] || fail "native golden missing: $golden"

    self_c="$BUILD_DIR/${name}_self.c"
    oracle_c="$BUILD_DIR/${name}_oracle.c"
    self_ok=0
    emit_via "${name}_self" "$SEED_BIN" "$src" "$self_c" && self_ok=1

    case "$status" in
        in_subset)
            if [[ "$self_ok" -ne 1 ]]; then
                echo "[$LABEL] '$name' REGRESSED out of the bootstrap subset." >&2
                echo "[$LABEL]   the Pergyra-built driver used to eat $src and no longer does:" >&2
                sed -n '1,3p' "$BUILD_DIR/${name}_self.err" >&2 || true
                grep -m 3 '^CODEGEN ERROR' "$self_c" >&2 || true
                exit 1
            fi
            emit_via "${name}_oracle" "$ORACLE_BIN" "$src" "$oracle_c" \
                || fail "'$name': oracle driver failed where the self-built one succeeded"
            pgy_selfhost_compare_expected_text_artifact_file_with_owner \
                "$LABEL:$name" "$BUILD_DIR" "$oracle_c" "$self_c" "emitted_c"
            "$CC" "$self_c" -o "$BUILD_DIR/${name}_self.exe" \
                2>"$BUILD_DIR/${name}_cc.err" \
                || { cat "$BUILD_DIR/${name}_cc.err" >&2; fail "'$name': self-emitted C failed to compile"; }
            (cd "$ROOT_DIR" && "$BUILD_DIR/${name}_self.exe" 2>"$BUILD_DIR/${name}_run.err" \
                | pgy_selfhost_normalize_text_artifact >"$BUILD_DIR/${name}_run.txt") \
                || { cat "$BUILD_DIR/${name}_run.err" >&2; fail "'$name': self-built binary failed"; }
            pgy_selfhost_compare_expected_text_artifact_file_with_owner \
                "$LABEL:$name-golden" "$BUILD_DIR" \
                "$ROOT_DIR/$golden" "$BUILD_DIR/${name}_run.txt" "run_output"
            in_rows=$((in_rows + 1))
            echo "[$LABEL] $name: in_subset ok (self==oracle emission, runs to the native golden)"
            ;;
        out_of_subset)
            if [[ "$self_ok" -eq 1 ]]; then
                echo "[$LABEL] '$name' is declared out_of_subset but the driver now EATS it." >&2
                echo "[$LABEL]   the bootstrap corpus grew -- good news that must be recorded:" >&2
                echo "[$LABEL]   promote the row to in_subset so the coverage is pinned." >&2
                exit 1
            fi
            controlled_refusal "$name" || {
                cat "$BUILD_DIR/${name}_self.out" \
                    "$BUILD_DIR/${name}_self.err" 2>/dev/null >&2 || true
                fail "'$name': bootstrap refusal was not a controlled CODEGEN ERROR"
            }
            out_rows=$((out_rows + 1))
            refusal="$(grep -m 1 -h 'CODEGEN ERROR' \
                "$BUILD_DIR/${name}_self.err" \
                "$BUILD_DIR/${name}_self.out" \
                "$self_c" 2>/dev/null || true)"
            echo "[$LABEL] $name: out_of_subset (recorded; ${refusal:-driver nonzero})"
            ;;
        *)
            fail "unknown declared status '$status' for $name"
            ;;
    esac
    rows=$((rows + 1))
done

[[ "$rows" -ge 3 ]] || fail "expected at least 3 corpus rows, saw $rows"

echo "[$LABEL] ok ($rows policy sources censused against the integrated driver:" \
     "$in_rows in_subset, $out_rows out_of_subset)"

#!/usr/bin/env bash
# Native bootstrap runtime regression only; not self-host MIR grant evidence.
set -Eeuo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
FIXTURE=tests/self_hosted/fixtures/json_owned_fragment_empty.pgy
INSTRUMENT=tests/self_hosted/parity/json_owned_fragment_empty_instrument.py
OBSERVER=tests/self_hosted/parity/json_owned_fragment_empty_observer.c
fail() { echo "[json-owned-empty] $*; evidence: ${WORK:-not-created}" >&2; exit 1; }
pgy_require_runnable_binary_here json-owned-empty "$PGY"
for tool in "$CC" "$CLANG" python timeout sha256sum cmp mktemp; do
    command -v "$tool" >/dev/null || fail "missing tool: $tool"
done
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/json-owned-empty.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
echo "[json-owned-empty] evidence: $REL"
sha256sum "$PGY" "$FIXTURE" "$INSTRUMENT" "$OBSERVER" \
    src/self_hosted/lib/json_emit.pgy >"$WORK/sources-before.sha256"
find src/runtime -type f -print0 | sort -z | xargs -0 sha256sum >"$WORK/runtime-before.sha256"
run_checked() {
    local label="$1"; shift
    if ! timeout 60 "$@" >"$WORK/$label.out" 2>"$WORK/$label.err"; then
        cat "$WORK/$label.out" "$WORK/$label.err" >&2
        fail "$label failed"
    fi
}
run_checked emit.c "$PGY" "$FIXTURE" --native-pipeline --emit-c -o "$REL/probe.c"
run_checked emit.llvm "$PGY" "$FIXTURE" --native-pipeline --emit-llvm -o "$REL/probe.ll"
run_checked runtime "$CLANG" -DPGY_LLVM_ENABLED -Dfree=pgy_json_observed_free \
    -Isrc -Isrc/runtime -pthread -c src/runtime/pgy_runtime_lib.c -o "$WORK/runtime.o"
run_checked observer "$CLANG" -std=c11 -c "$OBSERVER" -o "$WORK/observer.o"
printf 'owned-fragment-free: empty=1 text=1\n' >"$WORK/expected.out"
printf 'payload' >"$WORK/expected.payload"
for backend in c llvm; do
    extension=c; [[ "$backend" == c ]] || extension=ll
    for variant in positive old_guard; do
        run_checked "$backend.$variant.instrument" python "$INSTRUMENT" \
            "$backend" "$variant" "$WORK/probe.$extension" "$WORK/$backend.$variant.$extension"
        if [[ "$backend" == c ]]; then
            run_checked "$backend.$variant.compile" "$CC" -std=c11 -fwrapv -fno-strict-aliasing \
                -Isrc -Isrc/runtime -pthread "$WORK/$backend.$variant.c" "$WORK/observer.o" \
                -lm -o "$WORK/$backend.$variant.exe"
        else
            run_checked "$backend.$variant.compile" "$CLANG" -x ir "$WORK/$backend.$variant.ll" \
                -x none "$WORK/runtime.o" "$WORK/observer.o" -pthread -lm \
                -o "$WORK/$backend.$variant.exe"
        fi
        artifact="$REL/$backend.$variant.payload"
        if [[ "$variant" == positive ]]; then
            run_checked "$backend.$variant.run" "$WORK/$backend.$variant.exe" "$artifact"
            tr -d '\r' <"$WORK/$backend.$variant.run.out" >"$WORK/$backend.normalized"
            cmp "$WORK/expected.out" "$WORK/$backend.normalized"
            [[ ! -s "$WORK/$backend.$variant.run.err" ]] || fail "positive observer stderr"
        else
            status=0
            timeout 30 "$WORK/$backend.$variant.exe" "$artifact" \
                >"$WORK/$backend.$variant.run.out" 2>"$WORK/$backend.$variant.run.err" || status=$?
            [[ "$status" == 77 ]] || fail "old guard did not fail the free observer: $status"
            grep -Fxq 'observed_fragment_free_mismatch watches=2 empty=0 text=1' \
                "$WORK/$backend.$variant.run.err" || fail "old guard failed at another boundary"
        fi
        cmp "$WORK/expected.payload" "$ROOT_DIR/$artifact"
    done
done
sha256sum "$PGY" "$FIXTURE" "$INSTRUMENT" "$OBSERVER" \
    src/self_hosted/lib/json_emit.pgy >"$WORK/sources-after.sha256"
find src/runtime -type f -print0 | sort -z | xargs -0 sha256sum >"$WORK/runtime-after.sha256"
cmp "$WORK/sources-before.sha256" "$WORK/sources-after.sha256"
cmp "$WORK/runtime-before.sha256" "$WORK/runtime-after.sha256"
sha256sum "$WORK"/*.exe "$WORK"/*.c "$WORK"/*.ll "$WORK"/*.o >"$WORK/artifacts.sha256"
echo '[json-owned-empty] native C+LLVM: exact allocated-empty/text frees and output; old-guard leak rejected: PASS'

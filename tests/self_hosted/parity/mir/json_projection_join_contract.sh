#!/usr/bin/env bash
# The shared output sink depends on the existing StringJoin copy contract.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/json-join-contract.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
fail() { echo "[json-join-contract] $*; evidence=$REL" >&2; exit 1; }
positive=tests/self_hosted/fixtures/json_projection_join_result.pgy
printf 'hello-world\nempty\n' >"$WORK/expected"
for backend in c llvm; do
    for pipeline in native installed; do
        flags=()
        [[ "$pipeline" != native ]] || flags=(--native-pipeline)
        output="$REL/$pipeline-$backend.exe"
        if ! PGY_SELF_DRIVER_BIN="$DRIVER" "$PGY" "${flags[@]}" \
            "--backend=$backend" "$positive" -o "$output" \
            >"$WORK/$pipeline-$backend.compile" 2>&1; then
            cat "$WORK/$pipeline-$backend.compile" >&2; fail "$pipeline/$backend rejected the copying join";
        fi
        "$ROOT_DIR/$output" | tr -d '\r' >"$WORK/$pipeline-$backend.run"
        cmp "$WORK/expected" "$WORK/$pipeline-$backend.run" || fail "$pipeline/$backend copy changed"
        for name in join_borrow literal_drop; do
            negative="tests/self_hosted/fixtures/json_projection_${name}_negative.pgy"
            if PGY_SELF_DRIVER_BIN="$DRIVER" "$PGY" "${flags[@]}" \
                "--backend=$backend" "$negative" -o "$REL/$pipeline-$backend-$name.forged.exe" \
                >"$WORK/$pipeline-$backend-$name.refuse" 2>&1; then
                fail "$pipeline/$backend admitted $name"
            fi
            [[ ! -e "$WORK/$pipeline-$backend-$name.forged.exe" ]] || fail "a refused input published an executable"
            grep -Eiq 'borrow|ownership|moved|released|drop|alias' "$WORK/$pipeline-$backend-$name.refuse" ||
                fail "$pipeline/$backend lost the $name storage refusal"
        done
    done
done
echo "[json-join-contract] PASS: native/installed C/LLVM copy survives retirement; conditional loan and static free refused; evidence=$REL"

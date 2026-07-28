#!/usr/bin/env bash
# Pins the world -> zone -> object/tobject symbolic-query semantic seam. Query
# arguments are declaration identities, not ordinary runtime bindings. Native
# C/LLVM execute the contract; self-host proof stops at typed MIR because the
# domain-query backend lowering is the next explicit rung.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
DRIVER="${PGY_SELFHOST_PREBUILT_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}"
BUILD_DIR="${PGY_SELFHOST_WORLD_TOBJECT_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/world_tobject_query}"
POSITIVE="tests/self_hosted/parity/fixture/world_tobject_projection_query.pgy"
NEGATIVE="tests/self_hosted/parity/fixture/world_tobject_projection_query_missing.pgy"

PGY="$(pgy_select_optional_exe_binary "$PGY")"
DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
pgy_require_runnable_binary_here "self-host-world-tobject-query" "$PGY" || exit 1
pgy_require_runnable_binary_here "self-host-world-tobject-query" "$DRIVER" || exit 1
mkdir -p "$BUILD_DIR"

self_mir="$BUILD_DIR/self.mir.json"
self_err="$BUILD_DIR/self.err"
if ! (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$POSITIVE" \
    >"$self_mir" 2>"$self_err"); then
    cat "$self_mir" "$self_err" >&2
    echo "[self-host-world-tobject-query] self semantic/MIR reachability failed" >&2
    exit 1
fi
for fact in '"nominal_kind":"tobject","name":"BuyerPacket"' \
    '"field_kind":"tobject_slot"' \
    '"owner":"PaymentWorld"' \
    '"call_target_name":"HasZoneProjection"'; do
    grep -Fq "$fact" "$self_mir" || {
        echo "[self-host-world-tobject-query] missing self MIR fact: $fact" >&2
        exit 1
    }
done

for backend in c llvm; do
    native_exe="$BUILD_DIR/native-${backend}.exe"
    native_log="$BUILD_DIR/native-${backend}.log"
    if ! (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/$POSITIVE")" \
        "--backend=$backend" -o \
        "$(pgy_path_for_compiler "$PGY" "$native_exe")" \
        >"$native_log" 2>&1); then
        cat "$native_log" >&2
        echo "[self-host-world-tobject-query] native $backend compile failed" >&2
        exit 1
    fi
    "$native_exe" | tr -d '\r' >"$BUILD_DIR/native-${backend}.out"
done
cmp -s "$BUILD_DIR/native-c.out" "$BUILD_DIR/native-llvm.out" || {
    echo "[self-host-world-tobject-query] native C/LLVM output drifted" >&2
    diff -u "$BUILD_DIR/native-c.out" "$BUILD_DIR/native-llvm.out" >&2 || true
    exit 1
}
grep -Fxq 'true' "$BUILD_DIR/native-c.out" || {
    echo "[self-host-world-tobject-query] expected true world projection result" >&2
    cat "$BUILD_DIR/native-c.out" >&2
    exit 1
}

negative_out="$BUILD_DIR/negative.out"
negative_err="$BUILD_DIR/negative.err"
if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$NEGATIVE" \
    >"$negative_out" 2>"$negative_err"); then
    echo "[self-host-world-tobject-query] missing tobject identity was accepted" >&2
    exit 1
fi
grep -Fq 'Code: undefined_symbol' "$negative_out" "$negative_err" || {
    cat "$negative_out" "$negative_err" >&2
    echo "[self-host-world-tobject-query] missing identity diagnostic drifted" >&2
    exit 1
}
grep -Fq 'name: missingPacket' "$negative_out" "$negative_err" || {
    cat "$negative_out" "$negative_err" >&2
    echo "[self-host-world-tobject-query] missing identity fact drifted" >&2
    exit 1
}
[[ ! -s "$negative_out" ]] || ! grep -Fq '"schema":"pgy.mir.v1"' "$negative_out" || {
    echo "[self-host-world-tobject-query] negative emitted a partial MIR artifact" >&2
    exit 1
}

echo "[self-host-world-tobject-query] PASS"

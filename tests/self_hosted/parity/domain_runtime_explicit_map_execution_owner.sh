#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
fail() { echo "[self-host-parity:domain-runtime-explicit-map] $*" >&2; exit 1; }

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "domain-runtime-explicit-map" "$PGY" \
    || fail "PGY_BIN is not runnable"
PYTHON_BIN="${PYTHON:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    PYTHON_BIN="$(command -v python3 || command -v python || true)"
fi
[[ -n "$PYTHON_BIN" ]] || fail "python3/python is required"
CC_BIN="${CC:-gcc}"
command -v "$CC_BIN" >/dev/null 2>&1 || fail "C compiler is required"

FIXTURE_REL="tests/cases/backend_compare/zone_layer_projection_explicit_map_runtime/main.pgy"
FIXTURE="$ROOT_DIR/$FIXTURE_REL"
BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/domain_runtime_explicit_map_execution"
DRIVER="$BUILD_DIR/driver_rung2.exe"
NATIVE_MIR="$BUILD_DIR/native.mir.json"
SELF_MIR="$BUILD_DIR/self.mir.json"
SELF_C="$BUILD_DIR/self.mir.c"
DIRECT_C="$BUILD_DIR/self.direct.c"
mkdir -p "$BUILD_DIR"

if [[ -n "${PGY_SELFHOST_PREBUILT_DRIVER:-}" ]]; then
    DRIVER="$(pgy_select_optional_exe_binary "$PGY_SELFHOST_PREBUILT_DRIVER")"
    [[ -x "$DRIVER" ]] || fail "prebuilt driver is not runnable: $DRIVER"
else
    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_main.pgy")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$DRIVER")" \
        >"$BUILD_DIR/driver.compile.log" 2>&1) \
        || { cat "$BUILD_DIR/driver.compile.log" >&2; fail "driver build failed"; }
fi

(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle \
    "$(pgy_path_for_compiler "$PGY" "$FIXTURE")" \
    2>"$BUILD_DIR/native.mir.err" | tr -d '\r' >"$NATIVE_MIR") \
    || { cat "$BUILD_DIR/native.mir.err" >&2; fail "native MIR production failed"; }
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$FIXTURE_REL" \
    2>"$BUILD_DIR/self.mir.err" | tr -d '\r' >"$SELF_MIR") \
    || { cat "$SELF_MIR" "$BUILD_DIR/self.mir.err" >&2; fail "self MIR production failed"; }

"$PYTHON_BIN" - "$NATIVE_MIR" "$SELF_MIR" "$FIXTURE" "$BUILD_DIR" <<'PY'
import json, pathlib, sys
native_path, self_path, fixture_path, output_dir = sys.argv[1:]
expected = {
    ("Poisoned", "refresh", "view", "life", "bearer", "hp", "Long", "Int", True),
    ("TrustedLink", "publish", "packet", "label", "target", "name", "String", "String", True),
}
for path in (native_path, self_path):
    rows = json.loads(pathlib.Path(path).read_text(encoding="utf-8"))["domain_runtime_assignments"]["projection_members"]
    observed = {(
        row["owner_name"], row["operation"], row["projection_slot_name"],
        row["target_field_name"], row["source_slot_name"], row["source_path"],
        row["target_field_type_name"], row["source_leaf_type_name"], row["explicit_map"],
    ) for row in rows}
    assert observed == expected, (path, observed)
    for row in rows:
        assert len(row["source_path_segments"]) == 1
        assert row["source_path_segments"][0]["field_name"] == row["source_path"]
        assert row["source_path_segments"][0]["field_syntax_id"] > 0
source = pathlib.Path(fixture_path).read_text(encoding="utf-8")
variants = {
    "no-map": source.replace("refresh view from bearer map {\n        life <- hp;\n    }", "refresh view from bearer").replace("publish packet from target map {\n        label <- name;\n    }", "publish packet from target"),
    "type-mismatch": source.replace("life <- hp;", "life <- name;"),
    "missing-source": source.replace("life <- hp;", "life <- missing;"),
    "duplicate-target": source.replace("life <- hp;", "life <- hp;\n        life <- hp;"),
}
output = pathlib.Path(output_dir)
for name, text in variants.items():
    assert text != source
    (output / f"negative-{name}.pgy").write_text(text, encoding="utf-8")
PY

(cd "$ROOT_DIR" && "$DRIVER" --mir-json "${SELF_MIR#$ROOT_DIR/}" \
    >"$SELF_C" 2>"$BUILD_DIR/self.c.err") \
    || { cat "$SELF_C" "$BUILD_DIR/self.c.err" >&2; fail "valid MIR was rejected"; }
(cd "$ROOT_DIR" && "$DRIVER" "$FIXTURE_REL" --emit-c-verified \
    >"$DIRECT_C" 2>"$BUILD_DIR/self.direct.err") \
    || { cat "$DIRECT_C" "$BUILD_DIR/self.direct.err" >&2; fail "direct source entrypoint failed"; }
cmp -s "$SELF_C" "$DIRECT_C" || fail "direct source bypassed admitted MIR"
grep -Fq -- '(*self).poison.view.life = (*self).poison.bearer.hp;' "$SELF_C" \
    || fail "explicit refresh assignment was lost"
grep -Fq -- '(*self).trust.packet.label = (*self).trust.target.name;' "$SELF_C" \
    || fail "explicit publish assignment was lost"

for negative in "$BUILD_DIR"/negative-*.pgy; do
    name="$(basename "$negative" .pgy)"
    if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
        "${negative#$ROOT_DIR/}" >"$negative.out" 2>"$negative.err"); then
        fail "$name was accepted"
    fi
    grep -Eq 'mir_domain_runtime_assignment_invalid|MIR producer requires exact domain participant and projection assignment facts' \
        "$negative.out" "$negative.err" \
        || { cat "$negative.out" "$negative.err" >&2; fail "$name diagnostic drifted"; }
done

for backend in c llvm; do
    native_exe="$BUILD_DIR/native.$backend.exe"
    (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$FIXTURE")" \
        "--backend=$backend" -o "$(pgy_path_for_compiler "$PGY" "$native_exe")" \
        >"$BUILD_DIR/native.$backend.log" 2>&1) \
        || { cat "$BUILD_DIR/native.$backend.log" >&2; fail "native $backend build failed"; }
    "$native_exe" | tr -d '\r' >"$BUILD_DIR/native.$backend.run"
    [[ "$(cat "$BUILD_DIR/native.$backend.run")" == $'7\ndst' ]] \
        || { cat "$BUILD_DIR/native.$backend.run" >&2; fail "native $backend runtime drifted"; }
done

"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread "$SELF_C" \
    -o "$BUILD_DIR/self.exe"
"$BUILD_DIR/self.exe" | tr -d '\r' >"$BUILD_DIR/self.run"
[[ "$(cat "$BUILD_DIR/self.run")" == $'7\ndst' ]] || fail "self runtime drifted"
cmp -s "$BUILD_DIR/self.run" "$BUILD_DIR/native.c.run" \
    || fail "self/native C outputs differ"
cmp -s "$BUILD_DIR/self.run" "$BUILD_DIR/native.llvm.run" \
    || fail "self/native LLVM outputs differ"
echo "[self-host-parity:domain-runtime-explicit-map] PASS"

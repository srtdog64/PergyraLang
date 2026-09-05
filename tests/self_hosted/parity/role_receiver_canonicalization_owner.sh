#!/usr/bin/env bash
# Source receiver shape must survive both MIR canonicalization routes.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL=self-host-role-receiver-canonicalization
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
SOURCE=src/self_hosted/mir_lower/fixture/role_operator_dispatch.pgy
fail() { echo "[$LABEL] $*" >&2; exit 1; }
[[ -x "$PGY" && -x "$DRIVER" && -n "$PYTHON_BIN" ]] || fail 'missing compiler or Python'
command -v "$CC" >/dev/null || fail 'missing C compiler'
pgy_selfhost_select_emitted_c_compile_profile || fail 'invalid emitted-C profile'
mkdir -p "$ROOT_DIR/.tmp/self_hosted/role_receiver_canonicalization"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/role_receiver_canonicalization/run.XXXXXX")"
WORK_REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"

# The native command is explicitly an oracle, never a retry for a self failure.
"$PGY" --test-native-mir-json-oracle "$SOURCE" \
    >"$WORK/native.raw.json" 2>"$WORK/native.produce.err" || fail 'native MIR production failed'
tr -d '\r' <"$WORK/native.raw.json" >"$WORK/native.mir.json"
"$DRIVER" --emit-mir-json-verified "$SOURCE" \
    >"$WORK/self.mir.json" 2>"$WORK/self.produce.err" || fail 'self MIR production failed'

for lane in native self; do
    mode=--canonicalize-mir-json
    [[ "$lane" != native ]] || mode=--canonicalize-oracle-mir-json
    input_hash="$(sha256sum "$WORK/$lane.mir.json")"
    if ! "$DRIVER" "$mode" "$WORK_REL/$lane.mir.json" \
        >"$WORK/$lane.canonical.json" 2>"$WORK/$lane.canonical.err"; then
        cat "$WORK/$lane.canonical.json" "$WORK/$lane.canonical.err" >&2
        fail "$lane canonicalization failed"
    fi
    [[ "$(sha256sum "$WORK/$lane.mir.json")" == "$input_hash" ]] || fail 'canonicalization changed input'
    "$PYTHON_BIN" - "$WORK/$lane.canonical.json" <<'PY'
import json, pathlib, sys
document = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
assert document.get("schema") == "pgy.mir.v1"
roles = {row["name"] for row in document["decls"] if row["kind"] == "role"}
methods = [row for row in document["routines"] if row.get("owner") in roles]
assert len(methods) == 1
method = methods[0]
assert method["source_syntax_id"] > 0
receiver, rhs = method["params"]
assert receiver["name"] == "self" and "type" in receiver and receiver["type"] is None
assert rhs["name"] == "rhs" and rhs["type"] == "Int"
assert receiver["source_syntax_id"] > 0 and rhs["source_syntax_id"] > 0
assert receiver["source_syntax_id"] != rhs["source_syntax_id"]
PY
    "$DRIVER" --mir-json "$WORK_REL/$lane.canonical.json" \
        >"$WORK/$lane.c" 2>"$WORK/$lane.emit.err" || fail "$lane canonical MIR-C failed"
    compile=("$CC" -x c -std=c11 "${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[@]}")
    if pgy_selfhost_emitted_c_uses_runtime_headers "$WORK/$lane.c"; then
        compile+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
    fi
    compile+=("$WORK/$lane.c" -o "$WORK/$lane.exe")
    if ! "${compile[@]}" >"$WORK/$lane.build" 2>&1; then
        cat "$WORK/$lane.build" >&2
        fail "$lane generated C failed to compile"
    fi
    actual="$("$WORK/$lane.exe" | tr -d '\r')" || fail "$lane execution failed"
    [[ "$actual" == 123 ]] || fail "$lane role dispatch changed: $actual"
done

# Fix reconstruction; do not loosen source admission to accept typed/late self.
for shape in typed late; do
    source="tests/self_hosted/parity/fixture/${shape}_self_parameter_rejected.pgy"
    if "$DRIVER" --emit-c-artifact-verified "$source" "$WORK_REL/$shape.c" \
        >"$WORK/$shape.out" 2>"$WORK/$shape.err"; then
        fail "$shape source receiver was accepted"
    else
        status=$?
    fi
    [[ "$status" -eq 1 && ! -e "$WORK/$shape.c" ]] || fail "$shape refusal was not artifact-free exit 1"
    grep -Fq 'missing_fact: receiver_source_parameter_shape' \
        "$WORK/$shape.out" "$WORK/$shape.err" || fail "$shape source receiver lost its owned refusal"
done
echo "[$LABEL] native/self canonical receiver identity, exact 123, typed/late refusal: PASS ($WORK_REL)"

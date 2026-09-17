#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

fail() {
    echo "[pgy-math-red-team] $*" >&2
    exit 1
}

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        fail "python is required for registry admission"
    fi
fi

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
[[ -x "$PGY" ]] || fail "missing compiler binary: $PGY"
pgy_require_runnable_binary_here "pgy-math-red-team" "$PGY"

SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
SELF_DRIVER="$(pgy_select_optional_exe_binary "$SELF_DRIVER")"
[[ -x "$SELF_DRIVER" ]] || fail "missing self-host compiler binary: $SELF_DRIVER"
pgy_require_runnable_binary_here "pgy-math-red-team-self-host" "$SELF_DRIVER"
export PGY_SELF_DRIVER_BIN="$SELF_DRIVER"

CHECKER="$ROOT_DIR/scripts/pgy_math_registry_admission.py"
RECEIPT="$ROOT_DIR/stdlib/pgy_math_registry.receipt.json"
PROJECTION="$ROOT_DIR/stdlib/pgy_math_registry.pgy"
FIXTURE="$ROOT_DIR/tests/cases/pgy_math_verification_registry/main.pgy"

for required in "$CHECKER" "$RECEIPT" "$PROJECTION" "$FIXTURE"; do
    [[ -f "$required" ]] || fail "missing required artifact: $required"
done

"$PYTHON_BIN" "$CHECKER" --receipt "$RECEIPT" --projection "$PROJECTION"
"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
import importlib.util
import io
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
spec = importlib.util.spec_from_file_location(
    "pgy_math_registry_admission",
    root / "scripts" / "pgy_math_registry_admission.py",
)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class GuardedStream(io.BytesIO):
    def __init__(self):
        super().__init__()
        self.requests = []

    def read(self, size=-1):
        self.requests.append(size)
        if len(self.requests) != 1 or size != module.MAX_RECEIPT_BYTES + 1:
            raise AssertionError("read_bounded performed an unbounded or repeated read")
        return b"x" * size


class GuardedPath:
    def __init__(self, stream):
        self.stream = stream

    def open(self, mode):
        if mode != "rb":
            raise AssertionError("read_bounded did not open the artifact as bytes")
        return self.stream


stream = GuardedStream()
try:
    module.read_bounded(
        GuardedPath(stream), module.MAX_RECEIPT_BYTES, "registry receipt"
    )
except module.AdmissionError as exc:
    assert "exceeds" in str(exc)
else:
    raise AssertionError("oversized bounded read was admitted")
assert stream.requests == [module.MAX_RECEIPT_BYTES + 1]
PY
grep -Fq 'plan.program.runtime_abi.bool_to_string_id != 0 ||' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_llvm_foreign_declaration_owner.pgy" ||
    fail "self-host LLVM foreign declarations omit Bool-to-String malloc ownership"

TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
if pgy_binary_expects_windows_paths "$PGY"; then
    TMP_BASE="$ROOT_DIR/.tmp"
    mkdir -p "$TMP_BASE"
fi
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_math_red_team.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

cat > "$WORK_DIR/expected.txt" <<'EOF'
2
37
sha256:1bc6e88600e1fa66fd7ee13eb6ec5478a3e9d1513d97ea25c7ac2f63dc7a0275
true
true
false
EOF

for route in native self-host; do
    for backend in ${PGY_MATH_BACKENDS:-c llvm}; do
        output="$WORK_DIR/$route-$backend.out"
        artifact="$WORK_DIR/pgy_math_$route-$backend.exe"
        route_args=()
        if [[ "$route" == "native" ]]; then
            route_args+=(--native-pipeline)
        fi
        "$PGY" "${route_args[@]}" \
            "$(pgy_path_for_compiler "$PGY" "$FIXTURE")" \
            --backend="$backend" --run \
            -o "$(pgy_path_for_compiler "$PGY" "$artifact")" \
            >"$WORK_DIR/$route-$backend.stdout" \
            2>"$WORK_DIR/$route-$backend.stderr"
        grep -E '^(sha256:[0-9a-f]{64}|-?[0-9]+|true|false)$' \
            "$WORK_DIR/$route-$backend.stdout" > "$output" || true
        diff -u "$WORK_DIR/expected.txt" "$output" ||
            fail "$route $backend runtime registry output drifted"
    done
done

expect_reject() {
    local name="$1"
    local receipt="$2"
    local projection="$3"
    local reason="${4:-}"
    if "$PYTHON_BIN" "$CHECKER" --receipt "$receipt" \
        --projection "$projection" >"$WORK_DIR/$name.log" 2>&1; then
        fail "$name attack was admitted"
    fi
    grep -Fq '[pgy-math-admission] reject:' "$WORK_DIR/$name.log" ||
        fail "$name rejection did not carry an admission diagnostic"
    if [[ -n "$reason" ]]; then
        grep -Fq "$reason" "$WORK_DIR/$name.log" ||
            fail "$name rejection did not report the owned reason"
    fi
}

cp "$PROJECTION" "$WORK_DIR/bad_count.pgy"
"$PYTHON_BIN" - "$WORK_DIR/bad_count.pgy" <<'PY'
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
text = path.read_text(encoding="utf-8")
path.write_text(text.replace("return 37;", "return 36;", 1), encoding="utf-8")
PY
expect_reject "count_tamper" "$RECEIPT" "$WORK_DIR/bad_count.pgy"

cp "$PROJECTION" "$WORK_DIR/missing_contract.pgy"
"$PYTHON_BIN" - "$WORK_DIR/missing_contract.pgy" <<'PY'
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
text = path.read_text(encoding="utf-8")
text = text.replace(
    '        if value == "verification.function_contract" { return true; }\n',
    "",
    2,
)
text = text.replace(
    '        if value == "function_contract" { return true; }\n',
    "",
    1,
)
path.write_text(text, encoding="utf-8")
PY
expect_reject "contract_deletion" "$RECEIPT" "$WORK_DIR/missing_contract.pgy"

"$PYTHON_BIN" - "$RECEIPT" "$WORK_DIR/duplicate_key.json" <<'PY'
import pathlib
import sys

source = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
attack = source.replace(
    '  "sourceRepository":',
    '  "artifactSchema": "duplicate-shadow",\n  "sourceRepository":',
    1,
)
pathlib.Path(sys.argv[2]).write_text(attack, encoding="utf-8")
PY
expect_reject "duplicate_receipt_key" \
    "$WORK_DIR/duplicate_key.json" "$PROJECTION"

"$PYTHON_BIN" - "$RECEIPT" "$WORK_DIR/unadmitted_commit.json" <<'PY'
import json
import pathlib
import sys

receipt = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
receipt["sourceCommit"] = "0" * 40
pathlib.Path(sys.argv[2]).write_text(
    json.dumps(receipt, indent=2) + "\n", encoding="utf-8"
)
PY
expect_reject "unadmitted_source_commit" \
    "$WORK_DIR/unadmitted_commit.json" "$PROJECTION" \
    "PgyMath source commit is not admitted"

"$PYTHON_BIN" - "$RECEIPT" "$WORK_DIR/non_string_commit.json" <<'PY'
import json
import pathlib
import sys

receipt = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
receipt["sourceCommit"] = 7
pathlib.Path(sys.argv[2]).write_text(
    json.dumps(receipt, indent=2) + "\n", encoding="utf-8"
)
PY
expect_reject "non_string_source_commit" \
    "$WORK_DIR/non_string_commit.json" "$PROJECTION" \
    "sourceCommit must be an exact lowercase Git object id"

"$PYTHON_BIN" - "$RECEIPT" "$WORK_DIR/unadmitted_digest.json" <<'PY'
import json
import pathlib
import sys

receipt = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
receipt["registryDigest"] = "sha256:" + "0" * 64
pathlib.Path(sys.argv[2]).write_text(
    json.dumps(receipt, indent=2) + "\n", encoding="utf-8"
)
PY
expect_reject "unadmitted_registry_digest" \
    "$WORK_DIR/unadmitted_digest.json" "$PROJECTION" \
    "PgyMath registry digest is not admitted"

"$PYTHON_BIN" - "$ROOT_DIR" "$RECEIPT" \
    "$WORK_DIR/forged_receipt.json" "$WORK_DIR/forged_projection.pgy" <<'PY'
import importlib.util
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
spec = importlib.util.spec_from_file_location(
    "pgy_math_registry_admission",
    root / "scripts" / "pgy_math_registry_admission.py",
)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

receipt = json.loads(pathlib.Path(sys.argv[2]).read_text(encoding="utf-8"))
# Keep the admitted commit, registry digest, count, and proof identity intact.
# A verifier that trusts only receipt/projection agreement would accept this
# coordinated substitution of one concept in both artifacts.
receipt["templateIds"][0] = "core.set_image.forged"
receipt["lookupKeys"][0] = "core.set_image.forged"
pathlib.Path(sys.argv[3]).write_text(
    json.dumps(receipt, indent=2) + "\n", encoding="utf-8"
)
pathlib.Path(sys.argv[4]).write_text(
    module.render_projection(receipt), encoding="utf-8"
)
PY
expect_reject "coordinated_contract_forgery" \
    "$WORK_DIR/forged_receipt.json" "$WORK_DIR/forged_projection.pgy"

echo "[pgy-math-red-team] bounded reads, native/self-host C/LLVM parity, and 7 tamper attacks passed"

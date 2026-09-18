#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

fail() {
    echo "[pgy-math-adversarial] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
SELF_DRIVER="$(pgy_select_optional_exe_binary "$SELF_DRIVER")"
PGY_MATH_ROOT="${PGY_MATH_ROOT:-}"
NODE_BIN="${NODE_BIN:-node}"
PYTHON_BIN="${PYTHON_BIN:-python}"
PGY_EXEC_INT_DOMAIN="pergyra.int.wrapping.i32.v1"
PGY_VERIFY_INT_DOMAIN="lean.int.unbounded.v1"

if ! command -v lake.exe >/dev/null 2>&1 \
        && [[ -n "${USERPROFILE:-}" ]] \
        && command -v cygpath >/dev/null 2>&1; then
    ELAN_BIN="$(cygpath -u "$USERPROFILE")/.elan/bin"
    if [[ -d "$ELAN_BIN" ]]; then
        export PATH="$ELAN_BIN:$PATH"
    fi
fi
if [[ -d "/c/Program Files/Git/cmd" ]]; then
    export PATH="/c/Program Files/Git/cmd:$PATH"
fi

[[ -x "$PGY" ]] || fail "missing compiler binary: $PGY"
[[ -x "$SELF_DRIVER" ]] || fail "missing self-host compiler binary: $SELF_DRIVER"
[[ -n "$PGY_MATH_ROOT" && -d "$PGY_MATH_ROOT" ]] ||
    fail "PGY_MATH_ROOT must name the PgyMath checkout"
command -v git >/dev/null 2>&1 || fail "git is required"
ADMITTED_PGY_MATH_COMMIT="$(
    PYTHONPATH="$ROOT_DIR${PYTHONPATH:+:$PYTHONPATH}" \
        "$PYTHON_BIN" -c \
        'from scripts.pgy_math_registry_admission import EXPECTED_SOURCE_COMMIT; print(EXPECTED_SOURCE_COMMIT)'
)" || fail "could not read the admitted PgyMath commit owner"
OBSERVED_PGY_MATH_COMMIT="$(git -C "$PGY_MATH_ROOT" rev-parse HEAD 2>/dev/null)" ||
    fail "PGY_MATH_ROOT is not a Git checkout"
[[ "$OBSERVED_PGY_MATH_COMMIT" == "$ADMITTED_PGY_MATH_COMMIT" ]] ||
    fail "PgyMath checkout revision is not the admitted commit: $OBSERVED_PGY_MATH_COMMIT"
[[ -z "$(git -C "$PGY_MATH_ROOT" status --porcelain --untracked-files=all)" ]] ||
    fail "PgyMath checkout must be clean; uncommitted audit code is not evidence"
[[ -f "$PGY_MATH_ROOT/verify/adversarial-audit.mjs" ]] ||
    fail "the admitted PgyMath checkout does not publish verify/adversarial-audit.mjs"
command -v "$NODE_BIN" >/dev/null 2>&1 || fail "node is required"
command -v lake.exe >/dev/null 2>&1 || command -v lake >/dev/null 2>&1 ||
    fail "Lean lake is required for the PgyMath adversarial audit"
grep -Fq '`+ - *` overflow is **defined wraparound**, not UB' \
    "$ROOT_DIR/docs/semantics/11_arithmetic_ub_model.md" ||
    fail "the Pergyra wrapping-Int semantic owner drifted"
grep -Fq "$PGY_EXEC_INT_DOMAIN" \
    "$ROOT_DIR/docs/semantics/11_arithmetic_ub_model.md" ||
    fail "the Pergyra executable Int domain identity is not owned"
grep -Fq "$PGY_VERIFY_INT_DOMAIN" \
    "$ROOT_DIR/docs/semantics/18_pgy_math_adversarial_matrix.md" ||
    fail "the verifier Int domain identity is not documented"
export PGY_SELF_DRIVER_BIN="$SELF_DRIVER"

TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
if pgy_binary_expects_windows_paths "$PGY"; then
    TMP_BASE="$ROOT_DIR/.tmp"
    mkdir -p "$TMP_BASE"
fi
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_math_adversarial.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT
FINDINGS="$WORK_DIR/findings.tsv"
: >"$FINDINGS"

record() {
    local id="$1"
    local status="$2"
    local boundary="$3"
    local evidence="$4"
    printf '%s\t%s\t%s\t%s\n' "$id" "$status" "$boundary" "$evidence" \
        >>"$FINDINGS"
}

compile_and_run() {
    local fixture="$1"
    local route="$2"
    local backend="$3"
    local stem="$4"
    local args=()
    local artifact="$WORK_DIR/${stem}_${route}_${backend}.exe"
    if [[ "$route" == "native" ]]; then
        args+=(--native-pipeline)
    fi
    "$PGY" "${args[@]}" \
        "$(pgy_path_for_compiler "$PGY" "$fixture")" \
        --backend="$backend" --run \
        -o "$(pgy_path_for_compiler "$PGY" "$artifact")" \
        >"$WORK_DIR/${stem}_${route}_${backend}.stdout" \
        2>"$WORK_DIR/${stem}_${route}_${backend}.stderr"
}

check_integer_wrap() {
    local id="$1"
    local case_name="$2"
    local expected="$3"
    local fixture="$ROOT_DIR/tests/cases/pgy_math_adversarial/$case_name.pgy"
    for route in native self-host; do
        for backend in c llvm; do
            compile_and_run "$fixture" "$route" "$backend" "$case_name"
            grep -E '^-?[0-9]+$' \
                "$WORK_DIR/${case_name}_${route}_${backend}.stdout" \
                >"$WORK_DIR/${case_name}_${route}_${backend}.value" || true
            [[ "$(<"$WORK_DIR/${case_name}_${route}_${backend}.value")" == "$expected" ]] ||
                fail "$case_name did not expose the pinned wrapping result on $route/$backend"
        done
    done
    record "$id" "OPEN" \
        "$PGY_VERIFY_INT_DOMAIN versus $PGY_EXEC_INT_DOMAIN" \
        "$case_name wraps to $expected on native/self-host C/LLVM; no numeric-domain equivalence receipt exists"
}

check_rejected_identity() {
    local id="$1"
    local case_name="$2"
    local native_reason="$3"
    local fixture="$ROOT_DIR/tests/cases/pgy_math_adversarial/$case_name.pgy"
    local all_owned=1
    for route in native self-host; do
        for backend in c llvm; do
            local artifact="$WORK_DIR/${case_name}_${route}_${backend}.exe"
            local args=()
            if [[ "$route" == "native" ]]; then
                args+=(--native-pipeline)
            fi
            set +e
            "$PGY" "${args[@]}" \
                "$(pgy_path_for_compiler "$PGY" "$fixture")" \
                --backend="$backend" \
                -o "$(pgy_path_for_compiler "$PGY" "$artifact")" \
                >"$WORK_DIR/${case_name}_${route}_${backend}.stdout" \
                2>"$WORK_DIR/${case_name}_${route}_${backend}.stderr"
            local code=$?
            set -e
            [[ "$code" -ne 0 ]] || fail "$case_name compiled on $route/$backend"
            [[ ! -e "$artifact" ]] || fail "$case_name emitted an artifact on rejection"
            if ! grep -Fq "$native_reason" \
                    "$WORK_DIR/${case_name}_${route}_${backend}.stdout" \
                    "$WORK_DIR/${case_name}_${route}_${backend}.stderr"; then
                all_owned=0
            fi
        done
    done
    record "$id" "CLOSED" "identity collision execution" \
        "$case_name is rejected before artifact emission on all four routes"
    if [[ "$all_owned" -eq 1 ]]; then
        record "${id}-DIAG" "CLOSED" "identity collision diagnostic parity" \
            "all routes report the owned redeclaration diagnostic"
    else
        record "${id}-DIAG" "OPEN" "identity collision diagnostic parity" \
            "self-host routes reject safely but do not report the native redeclaration identity"
    fi
}

check_integer_wrap "PGY-ADV-RUNTIME-INT-01" "int_max_add" "-2147483648"
check_integer_wrap "PGY-ADV-RUNTIME-INT-02" "int_min_subtract" "2147483647"
check_integer_wrap "PGY-ADV-RUNTIME-INT-03" "int_min_negate" "-2147483648"

check_rejected_identity "PGY-ADV-IDENTITY-01" \
    "flattened_namespace_collision" "Redeclaration of function 'A_B_C'"
check_rejected_identity "PGY-ADV-IDENTITY-02" \
    "registry_shadow" "Redeclaration of function 'PgyMathRegistry_TemplateCount'"

STRESS_FIXTURE="$ROOT_DIR/tests/cases/pgy_math_adversarial/scalar_to_string_stress.pgy"
printf 'false\ntrue\nfalse\n42\nstable\n' >"$WORK_DIR/stress.expected"
for route in native self-host; do
    for backend in c llvm; do
        compile_and_run "$STRESS_FIXTURE" "$route" "$backend" "scalar_to_string_stress"
        grep -E '^(true|false|-?[0-9]+|stable)$' \
            "$WORK_DIR/scalar_to_string_stress_${route}_${backend}.stdout" \
            >"$WORK_DIR/stress_${route}_${backend}.actual" || true
        diff -u "$WORK_DIR/stress.expected" \
            "$WORK_DIR/stress_${route}_${backend}.actual" ||
            fail "scalar ToString stress drifted on $route/$backend"
    done
done
record "PGY-ADV-RUNTIME-ABI-01" "CLOSED" "scalar ToString ABI parity" \
    "10000 conversions plus Bool/Int/String output match on all four routes"
record "PGY-ADV-RUNTIME-LIFETIME-01" "UNMEASURED" \
    "scalar ToString allocation lifetime" \
    "parity passed, but the Windows gate has no leak-enabled sanitizer oracle"

grep -Fq 'layers["abi"].get("status") == "manifest-only"' \
    "$ROOT_DIR/tests/proof_carrying_pipeline_smoke.sh" ||
    fail "proof certificate ABI boundary changed; rerun this attack design"
grep -Fq '"id": "backend"' "$ROOT_DIR/tests/proof_carrying_pipeline_smoke.sh" ||
    fail "proof certificate backend layer is missing"
printf '{"layout":"v1"}\n' >"$WORK_DIR/abi-artifact.json"
printf 'backend-v1\n' >"$WORK_DIR/backend-artifact.bin"
"$PYTHON_BIN" - \
    "$ROOT_DIR/tests/cases/backend_compare/intent_zone_binding/main.pgy" \
    "$WORK_DIR/abi-artifact.json" "$WORK_DIR/backend-artifact.bin" <<'PY'
import hashlib
import json
import pathlib
import sys

source = pathlib.Path(sys.argv[1])
abi = pathlib.Path(sys.argv[2])
backend = pathlib.Path(sys.argv[3])

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def current_binding():
    payload = {
        "air_sha256": "a" * 64,
        "mir_sha256": "b" * 64,
        "schema": "pgy.proof-input-binding.v1",
        "source_sha256": sha(source),
    }
    encoded = json.dumps(payload, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(encoded.encode("utf-8")).hexdigest()

before = current_binding()
abi.write_text('{"layout":"attacker"}\n', encoding="utf-8")
backend.write_bytes(b"backend-attacker\n")
after = current_binding()
if before != after:
    raise SystemExit("ABI/backend mutation unexpectedly changed current binding")
PY
record "PGY-ADV-CERT-ABI-01" "OPEN" "post-certificate ABI substitution" \
    "mutating a stand-in ABI artifact preserved the current source/AIR/MIR composite digest"
record "PGY-ADV-CERT-BACKEND-01" "OPEN" "post-certificate backend substitution" \
    "mutating a stand-in backend artifact preserved the current source/AIR/MIR composite digest"
record "PGY-ADV-CERT-CONSUMER-01" "OPEN" "certificate TOCTOU at publication" \
    "the envelope is exercised by a smoke, not admitted by the production backend immediately before emission"

PGY_BIN="$PGY" "$ROOT_DIR/tests/proof_carrying_pipeline_smoke.sh" \
    >"$WORK_DIR/proof-pipeline.log" 2>&1
record "PGY-ADV-CERT-BINDING-01" "CLOSED" "stale source/AIR/MIR payload" \
    "the current envelope smoke rejects source, AIR, MIR and composite digest drift"

PGY_BIN="$PGY" PGY_SELF_DRIVER_BIN="$SELF_DRIVER" \
    "$ROOT_DIR/tests/pgy_math_registry_admission_smoke.sh" \
    >"$WORK_DIR/registry-admission.log" 2>&1
record "PGY-ADV-REGISTRY-01" "CLOSED" "registry replay and coordinated forgery" \
    "seven commit/digest/projection/JSON/type attacks and four-route parity pass"

"$NODE_BIN" "$PGY_MATH_ROOT/verify/adversarial-audit.mjs" \
    >"$WORK_DIR/pgy-math-audit.json"
"$PYTHON_BIN" - "$WORK_DIR/pgy-math-audit.json" <<'PY'
import json
import pathlib
import sys

report = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
expected = {"CLOSED": 7, "OPEN": 6, "UNMEASURED": 1}
if report.get("schema") != "pgy.verify.adversarial-audit.v1":
    raise SystemExit("unexpected PgyMath adversarial audit schema")
if report.get("counts") != expected:
    raise SystemExit(
        "PgyMath adversarial inventory drifted: {} != {}".format(
            report.get("counts"), expected
        )
    )
PY
record "PGY-ADV-CROSS-AUDIT-01" "CLOSED" "PgyMath verifier attack inventory" \
    "machine-readable audit observed 7 closed, 6 open and 1 unmeasured boundary"

"$PYTHON_BIN" - "$FINDINGS" <<'PY'
import json
import pathlib
import sys

rows = []
for line in pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").splitlines():
    identifier, status, boundary, evidence = line.split("\t")
    rows.append({
        "id": identifier,
        "status": status,
        "boundary": boundary,
        "evidence": evidence,
    })
counts = {
    status: sum(row["status"] == status for row in rows)
    for status in ("CLOSED", "OPEN", "UNMEASURED")
}
expected = {"CLOSED": 6, "OPEN": 8, "UNMEASURED": 1}
if counts != expected:
    raise SystemExit("Pergyra adversarial inventory drifted: {} != {}".format(
        counts, expected
    ))
print(json.dumps({
    "schema": "pgy.math.pergyra-adversarial-matrix.v1",
    "counts": counts,
    "findings": rows,
}, indent=2))
PY

echo "[pgy-math-adversarial] PASS inventory: 6 closed, 8 open, 1 unmeasured; open findings remain explicit"

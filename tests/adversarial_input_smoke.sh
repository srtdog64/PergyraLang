#!/usr/bin/env bash
# Adversarial-input gate (docs/189 C8/C10/C14): the compiler must terminate
# with a diagnostic -- never hang, never crash -- on hostile input. This is
# the executable exercise for the guards that otherwise only exist as code:
# the parser depth cap (400), the expression-operator cap (4096), the
# else-if chain cap (512), and the front-end's tolerance of garbage bytes
# and megabyte-scale sources.
# Complements tests/semantic_termination_security_smoke.sh (step budget +
# embedded NUL).
set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 \
    || ! command -v pwd >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi

if [[ ! -x "$PGY" ]]; then
    echo "missing compiler binary: $PGY" >&2
    exit 1
fi

WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pgy-adversarial.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

# Every case runs under a hard timeout: a hang (exit 124) is a gate
# failure in its own category, distinct from crash (>=128) and accept.
run_case() {
    local name="$1"
    local expect="$2"   # reject | accept | survive (either outcome; only hang/crash fail)
    local budget_s="$3"
    local rc

    # --native-pipeline: the guards under test (parser depth cap, operator
    # cap, else-if chain cap, AST node budget) are native front-end subjects;
    # with an installed sibling the default route would delegate to the
    # self-host driver and test a different compiler.
    set +e
    timeout "$budget_s" "$PGY" --native-pipeline \
        "$(pgy_path_for_compiler "$PGY" "$WORK_DIR/$name.pgy")" \
        --emit-c -o "$(pgy_path_for_compiler "$PGY" "$WORK_DIR/$name.c")" \
        >"$WORK_DIR/$name.log" 2>&1
    rc=$?
    set -e

    if [[ "$rc" -eq 124 ]]; then
        echo "[adversarial] $name HUNG past ${budget_s}s (termination contract broken)" >&2
        exit 1
    fi
    if [[ "$rc" -ge 128 ]]; then
        echo "[adversarial] $name crashed (signal exit $rc)" >&2
        tail -5 "$WORK_DIR/$name.log" >&2
        exit 1
    fi
    if [[ "$expect" == "reject" && "$rc" -eq 0 ]]; then
        echo "[adversarial] $name unexpectedly accepted" >&2
        exit 1
    fi
    if [[ "$expect" == "accept" && "$rc" -ne 0 ]]; then
        echo "[adversarial] $name unexpectedly rejected (rc=$rc)" >&2
        tail -5 "$WORK_DIR/$name.log" >&2
        exit 1
    fi
}

# 1. 500 nested blocks -- exercises the parser depth cap (400).
awk 'BEGIN {
    print "func Main() -> Void {";
    for (i = 0; i < 500; i++) print "    if true {";
    print "        Log(\"deep\");";
    for (i = 0; i < 500; i++) print "    }";
    print "}";
}' > "$WORK_DIR/deep_nesting.pgy"
run_case "deep_nesting" "reject" 20

# 2. 5000-operator chain -- exercises the expression-operator cap (4096).
awk 'BEGIN {
    printf "func Main() -> Void {\n    let x: Int = 1";
    for (i = 0; i < 5000; i++) printf " + 1";
    print ";\n    Log(ToString(x));\n}";
}' > "$WORK_DIR/operator_bomb.pgy"
run_case "operator_bomb" "reject" 20

# 2b. 600-arm else-if chain -- exercises the else-if chain cap (512). The
#     tail recursed through parse_if before the iterative driver landed, so
#     a 50,000-arm chain crashed the parser with no diagnostic instead of
#     reaching the semantic analyzer's 512-depth statement backstop.
awk 'BEGIN {
    print "func Classify(value: Int) -> Int {";
    print "    if value == 0 {";
    print "        return 0;";
    for (i = 1; i < 600; i++) {
        printf "    } else if value == %d {\n", i;
        printf "        return %d;\n", i;
    }
    print "    }";
    print "    return -1;";
    print "}";
    print "func Main() -> Void {";
    print "    Log(ToString(Classify(3)));";
    print "}";
}' > "$WORK_DIR/else_if_bomb.pgy"
run_case "else_if_bomb" "reject" 60
grep -q "too many chained else-if arms" "$WORK_DIR/else_if_bomb.log" || {
    echo "[adversarial] else_if_bomb rejected without the chain diagnostic" >&2
    exit 1
}

# 2c. 500-arm chain inside the cap: must be ACCEPTED end to end -- the cap
#     must not move the accept boundary the semantic backstop already drew.
awk 'BEGIN {
    print "func Classify(value: Int) -> Int {";
    print "    if value == 0 {";
    print "        return 0;";
    for (i = 1; i < 500; i++) {
        printf "    } else if value == %d {\n", i;
        printf "        return %d;\n", i;
    }
    print "    }";
    print "    return -1;";
    print "}";
    print "func Main() -> Void {";
    print "    Log(ToString(Classify(3)));";
    print "}";
}' > "$WORK_DIR/else_if_wide_valid.pgy"
run_case "else_if_wide_valid" "accept" 120

# 3. One-megabyte identifier token. There is deliberately no identifier
#    length cap (gcc has none either); the contract here is only
#    terminate-without-hang-or-crash.
awk 'BEGIN {
    printf "func Main() -> Void {\n    let ";
    for (i = 0; i < 1048576; i++) printf "a";
    print ": Int = 1;\n}";
}' > "$WORK_DIR/huge_token.pgy"
run_case "huge_token" "survive" 20

# 4. Deterministic garbage bytes (no NUL -- that case lives in the
#    termination-security smoke).
awk 'BEGIN {
    srand(42);
    for (i = 0; i < 2000; i++) {
        line = "";
        for (j = 0; j < 40; j++)
            line = line sprintf("%c", 33 + (i * 40 + j * 7) % 94);
        print line;
    }
}' > "$WORK_DIR/garbage.pgy"
run_case "garbage" "reject" 20

# 5. Large-but-valid source under the AST node budget (~120K statements,
#    ~600K nodes, ~4MB): must be ACCEPTED and finish inside the box --
#    termination is the contract, not rejection of size. This case also
#    pins the operator-cap granularity fix: before the expr-root reset
#    (docs/189 C14) the per-function accumulation spuriously rejected it
#    at statement ~4096.
awk 'BEGIN {
    print "func Main() -> Void {";
    print "    let mut total: Int = 0;";
    for (i = 0; i < 120000; i++) printf "    total = total + %d;\n", i % 7;
    print "    Log(ToString(total));";
    print "}";
}' > "$WORK_DIR/big_valid.pgy"
run_case "big_valid" "accept" 120

# 6. Past the 1M AST node budget (~300K statements): the bounded-refusal
#    path must reject CLEANLY (message + nonzero exit), not crash.
awk 'BEGIN {
    print "func Main() -> Void {";
    print "    let mut total: Int = 0;";
    for (i = 0; i < 300000; i++) printf "    total = total + %d;\n", i % 7;
    print "}";
}' > "$WORK_DIR/node_budget_bound.pgy"
run_case "node_budget_bound" "reject" 120
grep -q "AST node budget exceeded" "$WORK_DIR/node_budget_bound.log" || {
    echo "[adversarial] node_budget_bound rejected without the budget diagnostic" >&2
    exit 1
}

echo "[adversarial] deep-nesting, operator-bomb, else-if-chain, huge-token, garbage, big-valid, and node-budget inputs all terminate with the contracted outcome"

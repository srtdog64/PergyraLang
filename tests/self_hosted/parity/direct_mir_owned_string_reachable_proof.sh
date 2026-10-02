#!/usr/bin/env bash
# Real reached-owner work and cycle falsifiers. This probe is a typed owner
# unit boundary, not sealed GraphPlan admission or whole-driver bootstrap.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-owned-string-reachable-proof"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"
FIXTURE="tests/self_hosted/fixtures/direct_mir_owned_string_reachable_proof_probe.pgy"
RESTRICTED_FIXTURE="tests/self_hosted/semantic/fixture/owned_string_proof_storage_wrong_path_rejected.pgy"
OWNER="src/self_hosted/compiler/direct_mir_scalar_program_owned_string_result_fact_owner.pgy"
BUILD_TIMEOUT="${PGY_SELFHOST_PROBE_BUILD_TIMEOUT:-300}s"
STEP_TIMEOUT="${PGY_SELFHOST_PROBE_TIMEOUT:-45}s"
fail() { echo "[$LABEL] $*; evidence: ${WORK_DIR:-not-created}" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
for tool in "$CC" timeout mktemp sha256sum python; do
    command -v "$tool" >/dev/null 2>&1 || fail "missing tool: $tool"
done
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/owned_string_reachable_proof.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR"/}"
echo "[$LABEL] evidence: $WORK_DIR"
cd "$ROOT_DIR"
sha256sum "$FIXTURE" "$RESTRICTED_FIXTURE" "$OWNER" "$PGY" > "$WORK_DIR/source-before.sha256"
# Exact function/type spelling in a different module must not gain the new
# compiler-internal retirement authority. Refusal precedes C publication.
if timeout "$STEP_TIMEOUT" "$PGY" "$RESTRICTED_FIXTURE" \
    --native-pipeline --emit-c -o "$WORK_REL/wrong-path.c" \
    > "$WORK_DIR/wrong-path.out" 2> "$WORK_DIR/wrong-path.err"; then
    fail "wrong-path retirement impersonation was admitted"
else
    status="$?"
    [[ "$status" != 124 && "$status" != 137 ]] ||
        fail "wrong-path refusal timed out rather than rejecting"
fi
grep -Fq 'CompilerRetireArrayStorage is restricted to self-host storage lifetime owners' \
    "$WORK_DIR/wrong-path.out" "$WORK_DIR/wrong-path.err" || {
    cat "$WORK_DIR/wrong-path.out" "$WORK_DIR/wrong-path.err" >&2
    fail "wrong-path refusal lost its restricted-caller diagnostic"
}
[[ ! -e "$WORK_DIR/wrong-path.c" ]] ||
    fail "wrong-path refusal published a C artifact"
echo "[$LABEL] wrong-path retirement authority: rejected"
if ! timeout "$BUILD_TIMEOUT" "$PGY" "$FIXTURE" --native-pipeline --emit-c \
    -o "$WORK_REL/probe.c" > "$WORK_DIR/emit.out" 2> "$WORK_DIR/emit.err"; then
    cat "$WORK_DIR/emit.out" "$WORK_DIR/emit.err" >&2
    fail "native bootstrap probe emission failed"
fi
sha256sum "$FIXTURE" "$RESTRICTED_FIXTURE" "$OWNER" "$PGY" > "$WORK_DIR/source-after.sha256"
cmp -s "$WORK_DIR/source-before.sha256" "$WORK_DIR/source-after.sha256" ||
    fail "owner/probe/launcher changed during emission"

# Route generated-runtime frees through an observer and register only the two
# query-local Array<Int> backings immediately before their real drop call. This
# proves distinct storage reaches free exactly once without copying the owner.
python - "$WORK_DIR/probe.c" "$WORK_DIR/probe-watched.c" <<'PY'
import pathlib
import sys

source = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
include_marker = "#include <stdlib.h>\n"
if source.count(include_marker) != 1:
    raise SystemExit("generated C lost its unique stdlib include marker")
source = source.replace(
    include_marker,
    include_marker
    + "void pgy_test_observed_free(void *ptr);\n"
    + "void pgy_test_watch_scratch(void *ptr);\n"
    + "#define free pgy_test_observed_free\n",
    1,
)
drop = "    pgy_array_drop_Int(&rows);\n"
if source.count(drop) != 1:
    raise SystemExit("scratch retirement no longer has one exact Int backing drop")
source = source.replace(
    drop,
    "    pgy_test_watch_scratch(rows.data);\n" + drop,
    1,
)
# Observe in the same translation unit, after the real typed definitions. Do
# not redeclare owner functions with erased pointer/value parameter types.
source += '\n#undef free\n#include "call_counts.c"\n'
pathlib.Path(sys.argv[2]).write_text(source, encoding="utf-8")
PY

# Instrument actual generated-owner function entry points, never a second
# ownership implementation. BlockHasProcessExit is called for each inspected
# terminal block; the positive DAG has exactly two terminal blocks per body.
python - "$WORK_DIR/probe.c" "$WORK_DIR/call_symbols.h" <<'PY'
import pathlib
import re
import sys

source = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
definitions = re.findall(
    r"^(?:bool|int32_t|void) ([A-Za-z_][A-Za-z0-9_]*)\([^;\n]*\)\n\{",
    source, re.MULTILINE,
)
logical_names = (
    "DirectMirScalarProgramFunctionReturnsOwnedString",
    "DirectMirScalarProgramBlockHasProcessExit",
    "DirectMirScalarProgramOwnedStringDefinitionExpression",
    "DirectMirScalarProgramOwnedStringProofStorageRetire",
)
rows = []
for logical_name in logical_names:
    symbols = [symbol for symbol in definitions if symbol.endswith(logical_name)]
    if len(symbols) != 1:
        raise SystemExit("generated owner definition is missing or ambiguous: " + logical_name)
    rows.append("#define " + logical_name + " " + symbols[0] + "\n")
pathlib.Path(sys.argv[2]).write_text("".join(rows), encoding="utf-8")
PY
python - "$WORK_DIR/call_counts.c" <<'PY'
import pathlib
import sys
pathlib.Path(sys.argv[1]).write_text(r'''
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "call_symbols.h"
static uint64_t queries, terminals, definitions, retirements;
static void *scratch_backings[2];
static uint64_t scratch_registers, scratch_frees[2];
static bool scratch_invalid;
__attribute__((no_instrument_function))
void pgy_test_watch_scratch(void *ptr) {
    if (ptr == NULL || scratch_registers >= 2) {
        scratch_invalid = true;
        return;
    }
    for (uint64_t i = 0; i < scratch_registers; ++i) {
        if (scratch_backings[i] == ptr) scratch_invalid = true;
    }
    scratch_backings[scratch_registers++] = ptr;
}
__attribute__((no_instrument_function))
void pgy_test_observed_free(void *ptr) {
    for (uint64_t i = 0; i < scratch_registers; ++i) {
        if (scratch_backings[i] == ptr) ++scratch_frees[i];
    }
    free(ptr);
}
__attribute__((no_instrument_function))
void __cyg_profile_func_enter(void *callee, void *caller) {
    (void)caller;
    if (callee == (void *)DirectMirScalarProgramFunctionReturnsOwnedString) ++queries;
    if (callee == (void *)DirectMirScalarProgramBlockHasProcessExit) ++terminals;
    if (callee == (void *)DirectMirScalarProgramOwnedStringDefinitionExpression) ++definitions;
    if (callee == (void *)DirectMirScalarProgramOwnedStringProofStorageRetire) ++retirements;
}
__attribute__((no_instrument_function))
void __cyg_profile_func_exit(void *callee, void *caller) { (void)callee; (void)caller; }
__attribute__((destructor, no_instrument_function))
static void report_calls(void) {
    fprintf(stderr, "owned-result-query-entries=%llu terminal-inspections=%llu local-definition-queries=%llu scratch-retire-calls=%llu scratch-backing-registers=%llu scratch-backing-frees=%llu,%llu scratch-backing-invalid=%u\n",
        (unsigned long long)queries, (unsigned long long)terminals,
        (unsigned long long)definitions, (unsigned long long)retirements,
        (unsigned long long)scratch_registers,
        (unsigned long long)scratch_frees[0],
        (unsigned long long)scratch_frees[1],
        scratch_invalid ? 1u : 0u);
}
''', encoding="utf-8")
PY
export PGY_SELFHOST_CC_PROFILE=test
pgy_selfhost_select_emitted_c_compile_profile
if ! timeout "$BUILD_TIMEOUT" "$CC" -x c -std=gnu11 \
    "${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[@]}" -finstrument-functions \
    -Isrc -Isrc/runtime -pthread "$WORK_DIR/probe-watched.c" \
    -lm -o "$WORK_DIR/probe.exe" > "$WORK_DIR/compile.out" 2> "$WORK_DIR/compile.err"; then
    cat "$WORK_DIR/compile.err" >&2
    fail "instrumented owner probe compilation failed"
fi
sha256sum "$WORK_DIR/probe.c" "$WORK_DIR/probe-watched.c" \
    "$WORK_DIR/call_symbols.h" "$WORK_DIR/call_counts.c" \
    "$WORK_DIR/probe.exe" > "$WORK_DIR/artifacts.sha256"

check_case() {
    local layers="$1" mode="$2" expected="$3" terminals="$4"
    local stem="$mode-$layers"
    if ! timeout "$STEP_TIMEOUT" "$WORK_DIR/probe.exe" "$layers" "$mode" \
        > "$WORK_DIR/$stem.out" 2> "$WORK_DIR/$stem.err"; then
        cat "$WORK_DIR/$stem.out" "$WORK_DIR/$stem.err" >&2
        fail "$stem did not terminate successfully"
    fi
    [[ "$(tr -d '\r\n' < "$WORK_DIR/$stem.out")" == "$expected" ]] ||
        fail "$stem returned the wrong ownership decision"
    python - "$WORK_DIR/$stem.err" "$terminals" <<'PY'
import pathlib
import re
import sys
observed = re.search(r"owned-result-query-entries=(\d+) terminal-inspections=(\d+) local-definition-queries=(\d+) scratch-retire-calls=(\d+) scratch-backing-registers=(\d+) scratch-backing-frees=(\d+),(\d+) scratch-backing-invalid=(\d+)",
                     pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
if not observed or int(observed[1]) != 1:
    raise SystemExit("missing instrumentation or recursive public-query entry")
if int(observed[4]) != 2:
    raise SystemExit("query-local pending/state scratch did not reach both retirement boundaries")
if tuple(int(observed[index]) for index in range(5, 9)) != (2, 1, 1, 0):
    raise SystemExit("query-local pending/state scratch did not free two distinct backings exactly once")
expected = int(sys.argv[2])
if expected >= 0 and int(observed[2]) != expected:
    raise SystemExit("same reachable terminal body was re-inspected")
print("query_entries=" + observed[1] + " terminal_inspections=" + observed[2] +
      " local_definition_queries=" + observed[3] + " scratch_retire_calls=" + observed[4] +
      " scratch_backing_frees=" + observed[6] + "," + observed[7])
PY
    echo "[$LABEL] $stem: $expected"
}

for layers in 6 10 14; do check_case "$layers" dag true "$((layers * 2))"; done
check_case 14 cross-edge true 28
check_case 4096 chain true 4096
for mode in direct-cycle mutual-cycle alias-cycle borrowed-branch invalid-target \
    entrypoint-target negative-target wrong-return-type no-return process-exit \
    wrong-expression-type missing-terminal-expression missing-definition \
    duplicate-definition foreign-local; do
    check_case 4 "$mode" false -1
done
echo "[$LABEL] reached-owner work and negative decisions: PASS"

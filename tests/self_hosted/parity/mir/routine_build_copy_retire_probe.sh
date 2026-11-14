#!/usr/bin/env bash
# Bounded typed copy/retire evidence. This does not claim ordinary/intent
# source publication, fixed-point bootstrap, or whole-program admission.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="routine-build-copy-retire-probe"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"
FIXTURE="tests/self_hosted/fixtures/routine_build_copy_retire_probe.pgy"
ENTRY_FIXTURE="tests/self_hosted/fixtures/routine_entry_exact_rebind_probe.pgy"
OWNER="src/self_hosted/mir/routine_build_storage_lifetime_owner.pgy"
COPY_OWNER="src/self_hosted/mir/routine_cfg_append_owner.pgy"
ENTRY_OWNER="src/self_hosted/mir/routine_entry_owner.pgy"
BUILD_TIMEOUT="${PGY_SELFHOST_PROBE_BUILD_TIMEOUT:-300}s"
STEP_TIMEOUT="${PGY_SELFHOST_PROBE_TIMEOUT:-45}s"
fail() { echo "[$LABEL] $*; evidence: ${WORK_DIR:-not-created}" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
for tool in "$CC" timeout mktemp sha256sum python; do
    command -v "$tool" >/dev/null 2>&1 || fail "missing tool: $tool"
done
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/routine_build_copy_retire.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR"/}"
echo "[$LABEL] evidence: $WORK_DIR"
cd "$ROOT_DIR"
sha256sum "$FIXTURE" "$ENTRY_FIXTURE" "$OWNER" "$COPY_OWNER" \
    "$ENTRY_OWNER" "$PGY" > "$WORK_DIR/source-before.sha256"
if ! timeout "$BUILD_TIMEOUT" "$PGY" "$FIXTURE" --native-pipeline --emit-c \
    -o "$WORK_REL/probe.c" > "$WORK_DIR/emit.out" 2> "$WORK_DIR/emit.err"; then
    cat "$WORK_DIR/emit.out" "$WORK_DIR/emit.err" >&2
    fail "native bootstrap probe emission failed"
fi
if ! timeout "$BUILD_TIMEOUT" "$PGY" "$ENTRY_FIXTURE" --native-pipeline \
    --emit-c -o "$WORK_REL/routine-entry.c" \
    > "$WORK_DIR/routine-entry.emit.out" \
    2> "$WORK_DIR/routine-entry.emit.err"; then
    cat "$WORK_DIR/routine-entry.emit.out" \
        "$WORK_DIR/routine-entry.emit.err" >&2
    fail "routine-entry exact-rebind emission failed"
fi
python - "$WORK_DIR/routine-entry.c" <<'PY'
import pathlib
import re
import sys

source = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")

def body_of(name: str) -> str:
    pattern = re.compile(
        rf"\n[^\n;]*\bpgy_u_{re.escape(name)}\([^;]*?\)\n\{{",
        re.DOTALL,
    )
    matches = list(pattern.finditer(source))
    if len(matches) != 1:
        raise SystemExit(
            f"expected one emitted definition for {name}, found {len(matches)}"
        )
    start = matches[0].end() - 1
    depth = 0
    quote = None
    escaped = False
    line_comment = False
    block_comment = False
    i = start
    while i < len(source):
        char = source[i]
        next_char = source[i + 1] if i + 1 < len(source) else ""
        if line_comment:
            if char == "\n":
                line_comment = False
        elif block_comment:
            if char == "*" and next_char == "/":
                block_comment = False
                i += 1
        elif quote is not None:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
        elif char == "/" and next_char == "/":
            line_comment = True
            i += 1
        elif char == "/" and next_char == "*":
            block_comment = True
            i += 1
        elif char in ('"', "'"):
            quote = char
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[start : i + 1]
        i += 1
    raise SystemExit(f"unterminated emitted definition for {name}")

for routine in (
    "SelfMirRoutineFromInput",
    "SelfMirLowerTrackedStatementFromArtifact",
    "SelfMirLowerForFromArtifact",
):
    clones = len(re.findall(r"\bpgy_array_clone_[A-Za-z0-9_]+\s*\(",
                            body_of(routine)))
    if clones != 0:
        raise SystemExit(
            f"{routine} emitted {clones} whole-build array Clone calls"
        )
print("routine-entry exact-rebind clone projection: PASS")
PY
sha256sum "$FIXTURE" "$ENTRY_FIXTURE" "$OWNER" "$COPY_OWNER" \
    "$ENTRY_OWNER" "$PGY" > "$WORK_DIR/source-after.sha256"
cmp -s "$WORK_DIR/source-before.sha256" "$WORK_DIR/source-after.sha256" ||
    fail "probe/owners/launcher changed during emission"
export PGY_SELFHOST_CC_PROFILE=test
pgy_selfhost_select_emitted_c_compile_profile

# Test apparatus watches actual frees without replacing append/retire logic.
# Preload stdlib before the macro so its CRT declaration is not rewritten.
# The wrapper preserves the real free and observes only the three named leaves
# and their copied backings/shared role String during actual owner retirement.
python - "$WORK_DIR/probe.c" "$WORK_DIR/watched.c" <<'PY'
import pathlib
import sys
source = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
prefix = r'''
#include <stdlib.h>
static void *probe_source_backings[3], *probe_target_backings[3];
static void *probe_source_role_string, *probe_target_role_string;
static int probe_source_frees[3], probe_target_frees[3];
static int probe_source_role_frees, probe_target_role_frees, probe_watch;
static void ProbeObserveFree(void *pointer);
#define free ProbeObserveFree
#define main pgy_probe_original_main
'''
suffix = r'''
#undef free
#undef main
static void ProbeObserveFree(void *pointer) {
    if (probe_watch && pointer != NULL) {
        for (int i = 0; i < 3; ++i) {
            if (pointer == probe_source_backings[i]) ++probe_source_frees[i];
            if (pointer == probe_target_backings[i]) ++probe_target_frees[i];
        }
        if (pointer == probe_source_role_string) ++probe_source_role_frees;
        if (pointer == probe_target_role_string) ++probe_target_role_frees;
    }
    free(pointer);
}
int main(void) {
    SelfMirRoutineBuild source = pgy_u_RoutineBuildCopyRetireProbeSource();
    SelfMirCfgRows target = pgy_u_SelfMirAppendCfg(
        pgy_u_SelfMirCfgRowsEmpty(), source.cfg);
    probe_source_backings[0] = source.cfg.blocks.intent_roles.data;
    probe_source_backings[1] = source.cfg.instructions.binding_source_syntax_ids.data;
    probe_source_backings[2] = source.cfg.local_refs.source_statement_syntax_ids.data;
    probe_target_backings[0] = target.blocks.intent_roles.data;
    probe_target_backings[1] = target.instructions.binding_source_syntax_ids.data;
    probe_target_backings[2] = target.local_refs.source_statement_syntax_ids.data;
    for (int i = 0; i < 3; ++i) {
        if (probe_source_backings[i] == NULL || probe_target_backings[i] == NULL ||
            probe_source_backings[i] == probe_target_backings[i]) {
            fprintf(stderr, "copy backing identity failed at leaf=%d\n", i);
            return 1;
        }
    }
    probe_source_role_string = source.cfg.blocks.intent_roles.data[0];
    probe_target_role_string = target.blocks.intent_roles.data[0];
    if (probe_source_role_string == NULL || probe_target_role_string == NULL ||
        probe_source_role_string == probe_target_role_string) {
        fputs("role String did not follow the private-push copy contract\n", stderr);
        return 1;
    }
    probe_watch = 1;
    pgy_u_SelfMirRoutineBuildStorageRetireAfterLastConsumer(source);
    probe_watch = 0;
    for (int i = 0; i < 3; ++i) {
        if (probe_source_frees[i] != 1 || probe_target_frees[i] != 0) {
            fprintf(stderr, "retirement free count failed at leaf=%d source=%d target=%d\n",
                i, probe_source_frees[i], probe_target_frees[i]);
            return 1;
        }
    }
    if (probe_source_role_frees != 0 || probe_target_role_frees != 0 ||
        target.blocks.intent_roles.length != 1 ||
        strcmp(target.blocks.intent_roles.data[0], "entry") != 0 ||
        target.instructions.binding_source_syntax_ids.length != 1 ||
        target.instructions.binding_source_syntax_ids.data[0] != 701 ||
        target.local_refs.source_statement_syntax_ids.length != 1 ||
        target.local_refs.source_statement_syntax_ids.data[0] != 809) {
        fputs("copied role/IDs or shared String lifetime were corrupted\n", stderr);
        return 1;
    }
    fprintf(stderr, "source-leaf-frees=1,1,1 target-leaf-frees=0,0,0 private-role-frees=0,0\n");
    puts("routine-build-copy-retire-ready");
    return 0;
}
'''
pathlib.Path(sys.argv[2]).write_text(prefix + source + suffix, encoding="utf-8")
PY

for mode in plain watched; do
    source="$WORK_DIR/probe.c"
    [[ "$mode" != watched ]] || source="$WORK_DIR/watched.c"
    if ! timeout "$BUILD_TIMEOUT" "$CC" -x c -std=gnu11 \
        "${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[@]}" -Isrc -Isrc/runtime -pthread \
        "$source" -lm -o "$WORK_DIR/$mode.exe" \
        > "$WORK_DIR/$mode.compile.out" 2> "$WORK_DIR/$mode.compile.err"; then
        cat "$WORK_DIR/$mode.compile.err" >&2
        fail "$mode compilation failed"
    fi
    if ! timeout "$STEP_TIMEOUT" "$WORK_DIR/$mode.exe" \
        > "$WORK_DIR/$mode.run.out" 2> "$WORK_DIR/$mode.run.err"; then
        cat "$WORK_DIR/$mode.run.out" "$WORK_DIR/$mode.run.err" >&2
        fail "$mode execution failed"
    fi
    [[ "$(tr -d '\r\n' < "$WORK_DIR/$mode.run.out")" == routine-build-copy-retire-ready ]] ||
        fail "$mode copied evidence was not confirmed"
done
grep -Fq 'source-leaf-frees=1,1,1 target-leaf-frees=0,0,0 private-role-frees=0,0' \
    "$WORK_DIR/watched.run.err" || fail "actual free instrumentation was not observed"
sha256sum "$WORK_DIR/probe.c" "$WORK_DIR/plain.exe" "$WORK_DIR/watched.c" \
    "$WORK_DIR/watched.exe" > "$WORK_DIR/artifacts.sha256"
echo "[$LABEL] actual append/copy/retire evidence: PASS"

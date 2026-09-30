#!/usr/bin/env bash
# Actual body retirement and self-host Bool runtime/rewrite evidence; not
# whole-body admission, public source publication or fixed-point bootstrap.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="body-bundle-typed-retirement-probe"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"
BODY_FIXTURE="tests/self_hosted/fixtures/body_bundle_typed_retirement_probe.pgy"
RUNTIME_FIXTURE="tests/self_hosted/fixtures/array_bool_storage_runtime_probe.pgy"
BODY_OWNER="src/self_hosted/mir/body_type_bundle_storage_lifetime_owner.pgy"
RUNTIME_OWNER="src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy"
REWRITE_OWNER="src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy"
BUILD_TIMEOUT="${PGY_SELFHOST_PROBE_BUILD_TIMEOUT:-300}s"
STEP_TIMEOUT="${PGY_SELFHOST_PROBE_TIMEOUT:-45}s"
fail() { echo "[$LABEL] $*; evidence: ${WORK_DIR:-not-created}" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
for tool in "$CC" timeout mktemp sha256sum python; do
    command -v "$tool" >/dev/null 2>&1 || fail "missing tool: $tool"
done
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/body_bundle_typed_retirement.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR"/}"
echo "[$LABEL] evidence: $WORK_DIR"
cd "$ROOT_DIR"
sha256sum "$BODY_FIXTURE" "$RUNTIME_FIXTURE" "$BODY_OWNER" "$RUNTIME_OWNER" \
    "$REWRITE_OWNER" "$PGY" > "$WORK_DIR/source-before.sha256"
export PGY_SELFHOST_CC_PROFILE=test
pgy_selfhost_select_emitted_c_compile_profile

compile_c() {
    local name="$1" source="$2"
    if ! timeout "$BUILD_TIMEOUT" "$CC" -x c -std=gnu11 \
        "${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[@]}" -Isrc -Isrc/runtime -pthread \
        "$source" -lm -o "$WORK_DIR/$name.exe" \
        > "$WORK_DIR/$name.compile.out" 2> "$WORK_DIR/$name.compile.err"; then
        cat "$WORK_DIR/$name.compile.err" >&2
        fail "$name compilation failed"
    fi
}
run_checked() {
    local name="$1"
    if ! timeout "$STEP_TIMEOUT" "$WORK_DIR/$name.exe" \
        > "$WORK_DIR/$name.run.out" 2> "$WORK_DIR/$name.run.err"; then
        cat "$WORK_DIR/$name.run.out" "$WORK_DIR/$name.run.err" >&2
        fail "$name execution failed"
    fi
}

for stage in body runtime; do
    fixture="$BODY_FIXTURE"
    [[ "$stage" != runtime ]] || fixture="$RUNTIME_FIXTURE"
    if ! timeout "$BUILD_TIMEOUT" "$PGY" "$fixture" --native-pipeline --emit-c \
        -o "$WORK_REL/$stage.c" > "$WORK_DIR/$stage.emit.out" 2> "$WORK_DIR/$stage.emit.err"; then
        cat "$WORK_DIR/$stage.emit.out" "$WORK_DIR/$stage.emit.err" >&2
        fail "$stage native bootstrap emission failed"
    fi
    compile_c "$stage" "$WORK_DIR/$stage.c"
    run_checked "$stage"
done
[[ "$(tr -d '\r\n' < "$WORK_DIR/body.run.out")" == body-bundle-typed-retirement-ready ]] ||
    fail "typed body retirement did not preserve the shared payload"

# Observe real frees from the real lifetime owner, not a second leaf/drop
# solver. Watch exactly the twenty new columns and the shared zone String.
python - "$WORK_DIR/body.c" "$WORK_DIR/body-watched.c" <<'PY'
import pathlib
import sys
leaves = (
    "collection_ownership.receipts.function_syntax_ids",
    "collection_ownership.receipts.owner_node_ids",
    "collection_ownership.receipts.lanes",
    "collection_ownership.receipts.local_call_ordinals",
    "collection_ownership.receipts.effect_kinds",
    "collection_ownership.receipts.receiver_binding_syntax_ids",
    "collection_ownership.receipts.source_binding_syntax_ids",
    "zone_carriage.fresh_local_node_ids", "zone_carriage.resource_path_starts",
    "zone_carriage.resource_path_counts", "zone_carriage.resource_field_paths",
    "zone_carriage.mutable_borrow_parameter_node_ids",
    "capabilities.callable_node_ids", "capabilities.declared_masks",
    "capabilities.used_masks", "capabilities.exported_masks", "capabilities.deferred_uses",
    "capabilities.declared_effects", "capabilities.known_call_effects",
    "capabilities.unknown_call_effects",
)
prefix = r'''
#include <stdlib.h>
static void *probe_leaves[20], *probe_zone_payload;
static int probe_frees[20], probe_payload_frees, probe_watch;
static void BodyProbeObserveFree(void *pointer);
#define free BodyProbeObserveFree
#define main pgy_body_probe_original_main
'''
suffix = r'''
#undef free
#undef main
_Static_assert(__builtin_types_compatible_p(__typeof__(((SemanticAstBodyTypeBundle *)0)->capabilities.deferred_uses.data), bool *), "deferred uses must retain Bool storage");
_Static_assert(__builtin_types_compatible_p(__typeof__(((SemanticAstBodyTypeBundle *)0)->capabilities.unknown_call_effects.data), bool *), "unknown effects must retain Bool storage");
static void BodyProbeObserveFree(void *pointer) {
    if (probe_watch && pointer != NULL) {
        for (int i = 0; i < 20; ++i) if (pointer == probe_leaves[i]) ++probe_frees[i];
        if (pointer == probe_zone_payload) ++probe_payload_frees;
    }
    free(pointer);
}
int main(void) {
    SemanticAstBodyTypeBundle source = BodyBundleTypedRetirementProbeSource();
'''
suffix += "\n".join("    probe_leaves[%d] = source.%s.data;" % (index, leaf)
                    for index, leaf in enumerate(leaves))
suffix += r'''
    for (int i = 0; i < 20; ++i) {
        if (probe_leaves[i] == NULL) { fprintf(stderr, "unpopulated leaf=%d\n", i); return 1; }
        for (int prior = 0; prior < i; ++prior) {
            if (probe_leaves[prior] == probe_leaves[i]) {
                fprintf(stderr, "aliased leaf backings=%d,%d\n", prior, i); return 1;
            }
        }
    }
    probe_zone_payload = source.zone_carriage.resource_field_paths.data[0];
    probe_watch = 1;
    SelfMirBodyTypeBundleStorageRetireAfterProgramFacts(source);
    probe_watch = 0;
    for (int i = 0; i < 20; ++i) if (probe_frees[i] != 1) {
        fprintf(stderr, "body leaf=%d free-count=%d\n", i, probe_frees[i]); return 1;
    }
    if (probe_payload_frees != 0 || strcmp(probe_zone_payload, "zone.entry") != 0) {
        fputs("shared zone String was destroyed\n", stderr); return 1;
    }
    fprintf(stderr, "body-leaf-frees=20-once zone-payload-frees=0 bool-storage=concrete\n");
    puts("body-bundle-typed-retirement-ready");
    return 0;
}
'''
pathlib.Path(sys.argv[2]).write_text(prefix + pathlib.Path(sys.argv[1]).read_text(encoding="utf-8") + suffix,
                                    encoding="utf-8")
PY
compile_c body-watched "$WORK_DIR/body-watched.c"
run_checked body-watched
grep -Fq 'body-leaf-frees=20-once zone-payload-frees=0 bool-storage=concrete' \
    "$WORK_DIR/body-watched.run.err" || fail "body free observation was missing"

# Compile the exact runtime text emitted by the real Pergyra owner. Metadata
# also carries the actual semantic builtin rewrite, used for populated drops.
python - "$WORK_DIR/runtime.run.out" "$WORK_DIR/bool-runtime-watched.c" <<'PY'
import pathlib
import re
import sys
text = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
def symbol(key):
    match = re.search(r"/\* bool-" + key + r"=([A-Za-z_][A-Za-z0-9_]*) \*/", text)
    if not match:
        raise SystemExit("missing emitted Bool runtime symbol: " + key)
    return match[1]
array_type, new, push, drop = (symbol(key) for key in ("type", "new", "push", "drop"))
rewrite = re.search(r"/\* bool-rewrite=(.*?) \*/", text)
if not rewrite or re.fullmatch(re.escape(drop) + r"\(&flags\)", rewrite[1]) is None:
    raise SystemExit("semantic retirement rewrite does not use concrete Bool storage")
prefix = r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define PGY_RUNTIME_PANIC_CLASS_OOM 1
#define PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED 1
#define PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS 2
#define PGY_RUNTIME_PANIC_REASON_ARRAY_INDEX_OUT_OF_BOUNDS 2
#define PGY_RUNTIME_PANIC(kind, reason) abort()
static void *probe_bool_backing;
static int probe_bool_frees, probe_nonnull_frees;
static void BoolProbeObserveFree(void *pointer);
#define free BoolProbeObserveFree
'''
suffix = r'''
#undef free
static void BoolProbeObserveFree(void *pointer) {
    if (pointer != NULL) {
        ++probe_nonnull_frees;
        if (pointer == probe_bool_backing) ++probe_bool_frees;
    }
    free(pointer);
}
'''
suffix += "\n_Static_assert(__builtin_types_compatible_p(__typeof__(((%s *)0)->data), bool *), \"emitted Bool layout must not become Int\");\n" % array_type
suffix += r'''
int main(void) {
    ARRAY flags = NEW();
    PUSH(&flags, true);
    PUSH(&flags, false);
    if (flags.len != 2 || !flags.data[0] || flags.data[1]) return 1;
    probe_bool_backing = flags.data;
    REWRITE;
    if (probe_bool_frees != 1 || flags.data != NULL || flags.len != 0 || flags.cap != 0) return 1;
    REWRITE;
    if (probe_bool_frees != 1) return 1;
    ARRAY empty = NEW();
    DROP(&empty);
    DROP(NULL);
    if (empty.data != NULL || empty.len != 0 || empty.cap != 0 || probe_nonnull_frees != 1) return 1;
    flags.data = malloc(4 * sizeof(bool)); flags.len = 0; flags.cap = 4;
    if (flags.data == NULL) return 1;
    probe_bool_backing = flags.data; probe_bool_frees = 0;
    REWRITE;
    if (probe_bool_frees != 1 || probe_nonnull_frees != 2 || flags.data != NULL || flags.len != 0 || flags.cap != 0) return 1;
    DROP(&flags);
    if (probe_bool_frees != 1 || probe_nonnull_frees != 2) return 1;
    fprintf(stderr, "bool-drop populated=once empty=no-free null=safe allocated-empty=once reset=complete rewrite=actual\n");
    puts("array-bool-storage-runtime-ready");
    return 0;
}
'''
for token, value in (("ARRAY", array_type), ("NEW", new), ("PUSH", push), ("DROP", drop), ("REWRITE", rewrite[1])):
    suffix = re.sub(r"\b" + token + r"\b", lambda match, replacement=value: replacement, suffix)
pathlib.Path(sys.argv[2]).write_text(prefix + text + suffix, encoding="utf-8")
PY
compile_c bool-runtime-watched "$WORK_DIR/bool-runtime-watched.c"
run_checked bool-runtime-watched
grep -Fq 'bool-drop populated=once empty=no-free null=safe allocated-empty=once reset=complete rewrite=actual' \
    "$WORK_DIR/bool-runtime-watched.run.err" || fail "emitted Bool drop observation was missing"
sha256sum "$BODY_FIXTURE" "$RUNTIME_FIXTURE" "$BODY_OWNER" "$RUNTIME_OWNER" \
    "$REWRITE_OWNER" "$PGY" > "$WORK_DIR/source-after.sha256"
cmp -s "$WORK_DIR/source-before.sha256" "$WORK_DIR/source-after.sha256" ||
    fail "probe/owners/launcher changed during the gate"
sha256sum "$WORK_DIR/body.c" "$WORK_DIR/body.exe" "$WORK_DIR/body-watched.c" \
    "$WORK_DIR/body-watched.exe" "$WORK_DIR/runtime.c" "$WORK_DIR/runtime.exe" \
    "$WORK_DIR/bool-runtime-watched.c" "$WORK_DIR/bool-runtime-watched.exe" \
    > "$WORK_DIR/artifacts.sha256"
echo "[$LABEL] actual body retirement and emitted Bool runtime/rewrite: PASS"

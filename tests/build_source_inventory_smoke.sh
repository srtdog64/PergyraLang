#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAKE_BIN="${MAKE:-}"

if [[ -z "$MAKE_BIN" ]]; then
    if command -v make >/dev/null 2>&1; then
        MAKE_BIN="$(command -v make)"
    elif command -v mingw32-make >/dev/null 2>&1; then
        MAKE_BIN="$(command -v mingw32-make)"
    else
        echo "[build-source-inventory] missing make" >&2
        exit 1
    fi
fi

inventory="$("$MAKE_BIN" -C "$ROOT_DIR" -s __pgy_build_source_inventory_print)"
missing=0

duplicate_sources="$(
    printf '%s\n' "$inventory" \
        | sed '/^$/d' \
        | sort \
        | uniq -d
)"
if [[ -n "$duplicate_sources" ]]; then
    printf '%s\n' "$duplicate_sources" >&2
    echo "[build-source-inventory] duplicate source inventory entries" >&2
    missing=1
fi

ignored_inventory=""
if command -v git >/dev/null 2>&1 \
    && git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    ignored_inventory="$(
        printf '%s\n' "$inventory" \
            | git -C "$ROOT_DIR" check-ignore --stdin --no-index 2>/dev/null \
            || true
    )"
fi

while IFS= read -r src; do
    [[ -n "$src" ]] || continue

    if [[ ! -f "$ROOT_DIR/$src" ]]; then
        echo "[build-source-inventory] missing required build file: $src" >&2
        missing=1
        continue
    fi

    if [[ -n "$ignored_inventory" ]] \
        && grep -Fxq "$src" <<< "$ignored_inventory"; then
        echo "[build-source-inventory] required build file is ignored: $src" >&2
        missing=1
    fi

    if [[ "${PGY_REQUIRE_TRACKED_SOURCES:-0}" == "1" ]] \
        && command -v git >/dev/null 2>&1 \
        && git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1 \
        && ! git -C "$ROOT_DIR" ls-files --error-unmatch "$src" >/dev/null 2>&1; then
        echo "[build-source-inventory] required build file is not tracked: $src" >&2
        missing=1
    fi
done <<< "$inventory"

if command -v git >/dev/null 2>&1 \
    && git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    deleted_examples="$(
        git -C "$ROOT_DIR" ls-files --deleted -- 'tests/cases' 'examples'
    )"
    while IFS= read -r tracked; do
        [[ -n "$tracked" ]] || continue
        if [[ -n "$deleted_examples" ]] \
            && grep -Fxq "$tracked" <<< "$deleted_examples"; then
            continue
        fi
        case "$tracked" in
            *.exe|*.dll|*.so|*.dylib|*.o|*.obj|*.a|*.lib|*.wasm)
                echo "[build-source-inventory] tracked executable artifact is not allowed: $tracked" >&2
                missing=1
                ;;
        esac
    done < <(git -C "$ROOT_DIR" ls-files 'tests/cases' 'examples')
fi

typo_tokens="$(
    grep -RInE '\b(retun|stncmp|retun_type|infer_spawn_retun)\b' \
        "$ROOT_DIR/src" \
        --include='*.c' --include='*.h' || true
)"
typo_tokens="$(
    printf '%s\n' "$typo_tokens" \
        | grep -v '/src/test_' \
        | grep -v '/src/tests/' || true
)"
if [[ -n "$typo_tokens" ]]; then
    printf '%s\n' "$typo_tokens" >&2
    echo "[build-source-inventory] production source contains typo-like C tokens" >&2
    missing=1
fi

if ! grep -Fq '$(BUILD_DIR)/compiler/hir_callgraph.o' "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] HIR callgraph source is not linked by HIR_CORE_OBJECTS" >&2
    missing=1
fi

if ! grep -Fq '#include "transpiler_expr_party_instance_emit.h"' \
    "$ROOT_DIR/src/codegen/transpiler_expr_emitters.h"; then
    echo "[build-source-inventory] party-instance expression emitter is not linked by the C expression emitter include chain" >&2
    missing=1
fi

if ! grep -Fq '#include "transpiler_let_type_register_emit.h"' \
    "$ROOT_DIR/src/codegen/transpiler_base_a_emitters.h"; then
    echo "[build-source-inventory] let type-registration emitter is not linked by the C let emitter include chain" >&2
    missing=1
fi

if ! grep -Fq '#include "transpiler_collection_runtime_suffix.h"' \
    "$ROOT_DIR/src/codegen/transpiler_helpers_core_b.h"; then
    echo "[build-source-inventory] collection runtime suffix helper is not linked by the C helper include chain" >&2
    missing=1
fi

if ! grep -Fq '#include "transpiler_roster_decl_emit.h"' \
    "$ROOT_DIR/src/codegen/transpiler_domain_nominal_emit.h"; then
    echo "[build-source-inventory] roster declaration emitter is not linked by the C domain nominal include chain" >&2
    missing=1
fi

if ! grep -Fq '#include "transpiler_zone_specialization_emit.h"' \
    "$ROOT_DIR/src/codegen/transpiler_zone_decl_emit.h"; then
    echo "[build-source-inventory] zone specialization emitter is not linked by the C zone include chain" >&2
    missing=1
fi

if ! grep -Fq '#include "transpiler_zone_methods_emit.h"' \
    "$ROOT_DIR/src/codegen/transpiler_zone_decl_emit.h"; then
    echo "[build-source-inventory] zone hosted-method emitter is not linked by the C zone include chain" >&2
    missing=1
fi

if ! grep -Fq '#include "transpiler_expr_stdlib_queue_builtin.h"' \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_collection_builtin.c"; then
    echo "[build-source-inventory] stdlib queue builtin emitter is not linked by the C collection builtin include chain" >&2
    missing=1
fi

if [[ "$missing" -ne 0 ]]; then
    exit 1
fi

echo "[build-source-inventory] Makefile source inventory ok"

#!/usr/bin/env bash
set -euo pipefail

# Subject of this gate:
#   CFG body dataflow changed.
# That is a fact about the native pipeline, so the gate compiles
# in-process instead of delegating to the installed self-host driver.
# Delegated, a self-host coverage gap would read as a regression in
# the subject above. Declared per harness because the compiler is
# reached through make and nested scripts, and the variable is the
# same declared opt-out as --native-pipeline -- never a fallback.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PYTHON_BIN="${PYTHON_BIN:-}"
DOCS_CHECK_DONE=0

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        DOCS_CHECK_DONE=1
    fi
fi

if [[ "$DOCS_CHECK_DONE" -eq 0 ]]; then
"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
mir_path = root / "src" / "compiler" / "mir.c"
mir_hir_block_projection_path = root / "src" / "compiler" / "mir_hir_block_projection.c"
mir_ssa_rename_path = root / "src" / "compiler" / "mir_ssa_rename.h"
mir_ssa_rename_impl_path = root / "src" / "compiler" / "mir_ssa_rename.c"
mir_ssa_use_edges_path = root / "src" / "compiler" / "mir_ssa_use_edges.c"
mir_liveness_dce_header_path = root / "src" / "compiler" / "mir_liveness_dce.h"
mir_liveness_dce_path = root / "src" / "compiler" / "mir_liveness_dce.c"
mir_liveness_summary_path = root / "src" / "compiler" / "mir_liveness_summary.c"
mir_dce_header_path = root / "src" / "compiler" / "mir_dce.h"
mir_dce_path = root / "src" / "compiler" / "mir_dce.c"
mir_call_fact_path = root / "src" / "compiler" / "mir_call_fact.h"
mir_call_fact_impl_path = root / "src" / "compiler" / "mir_call_fact.c"
mir_stmt_population_header_path = root / "src" / "compiler" / "mir_stmt_population.h"
mir_stmt_population_path = root / "src" / "compiler" / "mir_stmt_population.c"
mir_stmt_population_resource_ops_path = root / "src" / "compiler" / "mir_stmt_population_resource_ops.c"
mir_stmt_source_path = root / "src" / "compiler" / "mir_stmt_source.c"
mir_non_cfg_stmt_population_path = root / "src" / "compiler" / "mir_non_cfg_stmt_population.h"
mir_non_cfg_stmt_population_impl_path = root / "src" / "compiler" / "mir_non_cfg_stmt_population.c"

mir_owner_limits = {
    mir_path: 600,
    mir_hir_block_projection_path: 600,
    mir_ssa_rename_path: 600,
    mir_ssa_rename_impl_path: 600,
    mir_ssa_use_edges_path: 600,
    mir_liveness_dce_header_path: 600,
    mir_liveness_dce_path: 600,
    mir_liveness_summary_path: 600,
    mir_dce_header_path: 600,
    mir_dce_path: 600,
    mir_call_fact_path: 600,
    mir_call_fact_impl_path: 600,
    mir_stmt_population_header_path: 600,
    mir_stmt_population_path: 600,
    mir_stmt_population_resource_ops_path: 600,
    mir_stmt_source_path: 600,
    mir_non_cfg_stmt_population_path: 600,
    mir_non_cfg_stmt_population_impl_path: 600,
}
for path, limit in mir_owner_limits.items():
    loc = len(path.read_text(encoding="utf-8").splitlines())
    if loc > limit:
        raise SystemExit(
            f"MIR CFG/body owner {path.relative_to(root)} is {loc} LOC; "
            f"split-review limit is {limit}"
        )
PY
fi

DEFAULT_PGY="$ROOT_DIR/bin/pgy"
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
TMP_PGY="${TMP_BASE%/}/pgy-PergyraLang-bin/pgy"

if pgy_binary_expects_windows_paths "${DEFAULT_PGY}.exe"; then
    DEFAULT_PGY="${DEFAULT_PGY}.exe"
fi
if pgy_binary_expects_windows_paths "${TMP_PGY}.exe"; then
    TMP_PGY="${TMP_PGY}.exe"
fi
PGY_EXPLICIT=0
if [[ -n "${PGY_BIN:-}" ]]; then
    PGY="$PGY_BIN"
    PGY_EXPLICIT=1
elif [[ -x "$DEFAULT_PGY" ]]; then
    PGY="$DEFAULT_PGY"
elif [[ -x "$TMP_PGY" ]]; then
    PGY="$TMP_PGY"
else
    PGY="$DEFAULT_PGY"
fi

EXAMPLE="${1:-$ROOT_DIR/tests/cases/backend_compare/intent_zone_binding/main.pgy}"
WORK_BASE="$ROOT_DIR/.tmp/cfg-body-dataflow"
mkdir -p "$WORK_BASE"
WORK_DIR="$(mktemp -d "$WORK_BASE.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

to_native_path_for_pgy() {
    pgy_path_for_compiler "$PGY" "$1"
}

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 1 ]]; then
        echo "cfg body dataflow missing compiler binary: $PGY" >&2
        exit 1
    fi
    echo "cfg-body-dataflow smoke: SKIP executable probe; source/doc contract already checked"
    exit 0
fi

if [[ ! -f "$EXAMPLE" ]]; then
    echo "missing example source: $EXAMPLE" >&2
    exit 1
fi

if ! "$PGY" --help >"$WORK_DIR/pgy-help.out" 2>"$WORK_DIR/pgy-help.err"; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "cfg-body-dataflow smoke: SKIP default compiler executable probe; source/doc contract already checked"
        exit 0
    fi
    echo "cfg body dataflow compiler binary is not runnable: $PGY" >&2
    cat "$WORK_DIR/pgy-help.err" >&2
    exit 1
fi

EXAMPLE_FOR_PGY="$(to_native_path_for_pgy "$EXAMPLE")"

HIR_CFG_OUT="$WORK_DIR/hir_cfg.txt"
HIR_DOM_OUT="$WORK_DIR/hir_dom.txt"
RIR_OUT="$WORK_DIR/rir.txt"
MIR_OUT="$WORK_DIR/mir.txt"
PARALLEL_SELECT="$WORK_DIR/parallel_select.pgy"
PARALLEL_SELECT_AST="$WORK_DIR/parallel_select_ast.txt"
PARALLEL_SELECT_MIR="$WORK_DIR/parallel_select_mir.txt"
WITH_SLOT_ORDER="$WORK_DIR/with_slot_order.pgy"
WITH_SLOT_ORDER_C="$WORK_DIR/with_slot_order.c"

"$PGY" "$EXAMPLE_FOR_PGY" --native-pipeline --hir-cfg > "$HIR_CFG_OUT"
"$PGY" "$EXAMPLE_FOR_PGY" --native-pipeline --hir-dom > "$HIR_DOM_OUT"
"$PGY" "$EXAMPLE_FOR_PGY" --native-pipeline --rir > "$RIR_OUT"
"$PGY" "$EXAMPLE_FOR_PGY" --native-pipeline --mir > "$MIR_OUT"

cat > "$PARALLEL_SELECT" <<'EOF'
func Main() -> Void {
    let ch: Channel<Int> = Channel(4);
    parallel {
        ch <- 7;
    }
    select {
        case v = <-ch:
            Log(v);
        default:
            Log(0);
    }
}
EOF
PARALLEL_SELECT_FOR_PGY="$(to_native_path_for_pgy "$PARALLEL_SELECT")"
"$PGY" "$PARALLEL_SELECT_FOR_PGY" --native-pipeline --ast > "$PARALLEL_SELECT_AST"
"$PGY" "$PARALLEL_SELECT_FOR_PGY" --native-pipeline --mir > "$PARALLEL_SELECT_MIR"

cat > "$WITH_SLOT_ORDER" <<'EOF'
func Cost() -> Int {
    return 4;
}

func Main() -> Void {
    with slot<Int> as s {
        Write(s, Cost());
        Print(ToString(Read(s)));
    }
}
EOF
WITH_SLOT_ORDER_FOR_PGY="$(to_native_path_for_pgy "$WITH_SLOT_ORDER")"
WITH_SLOT_ORDER_C_FOR_PGY="$(to_native_path_for_pgy "$WITH_SLOT_ORDER_C")"
"$PGY" "$WITH_SLOT_ORDER_FOR_PGY" --emit-c -o "$WITH_SLOT_ORDER_C_FOR_PGY" > "$WORK_DIR/with_slot_order.out"

grep -Fq "HIR cfg view" "$HIR_CFG_OUT"
grep -Fq "intent SyncDrive" "$HIR_CFG_OUT"
grep -Fq "blocks=2" "$HIR_CFG_OUT"
grep -Fq "succ=T" "$HIR_CFG_OUT"

grep -Fq "HIR dom view" "$HIR_DOM_OUT"
grep -Fq "idom=" "$HIR_DOM_OUT"
grep -Fq "df=" "$HIR_DOM_OUT"
grep -Fq "loop=" "$HIR_DOM_OUT"
grep -Fq "rpo=" "$HIR_DOM_OUT"

grep -Fq "flow-block[" "$RIR_OUT"
grep -Fq "semantics=authority|projection" "$RIR_OUT"
grep -Fq "semantics=authority|invalidation|authority-loss" "$RIR_OUT"
grep -Fq "kind=ProjectionObject state=Dirty" "$RIR_OUT"

grep -Fq "routine[01] intent   SyncDrive blocks=5" "$MIR_OUT"
grep -Fq "noncfg-fallbacks: total=0 routines=0 recorded=yes" "$MIR_OUT"
grep -Fq "value[00] cockpit.1 slot=cockpit" "$MIR_OUT"
grep -Fq "cleanup-block=yes rollback-block=yes invalidation-block=yes" "$MIR_OUT"
grep -Fq "cleanup-edge" "$MIR_OUT"
grep -Fq "DetachInvalidation" "$MIR_OUT"

grep -Fq "Parallel:" "$PARALLEL_SELECT_AST"
grep -Fq "ChannelSend: ch <- 7" "$PARALLEL_SELECT_AST"
grep -Fq "inst[01] stmt" "$PARALLEL_SELECT_MIR"
grep -Fq "ast=AST_PARALLEL_BLOCK" "$PARALLEL_SELECT_MIR"

claim_line="$(grep -n "PgySlot_Int s = pgy_claim_Int();" "$WITH_SLOT_ORDER_C" | head -1 | cut -d: -f1)"
read_line="$(grep -n "pgy_read_Int(&s)" "$WITH_SLOT_ORDER_C" | head -1 | cut -d: -f1)"
if [[ -z "$claim_line" || -z "$read_line" || "$claim_line" -ge "$read_line" ]]; then
    echo "with-slot MIR resource ops must preserve source order before residual Read statements" >&2
    exit 1
fi

# Corpus-wide non-CFG fallback invariant.  Every routine kind --- plain
# function, class method, intent, role method, party method, event handler
# --- must lower through the CFG path.  The AST body fallback
# (mir_append_non_cfg_body_statements) must never fire.  A non-zero count
# here means a construct stopped emitting CFG and silently regressed to
# AST-driven body population, which breaks the MIR source-of-truth contract.
NON_CFG_CORPUS=(
    "examples/surface_compression_maximal.pgy"
    "examples/composite_intent_orchestration_compressed.pgy"
    "examples/lambda_test.pgy"
    "examples/dnd_tavern_campaign/combat_cards.pgy"
    "examples/dnd_tavern_campaign/events.pgy"
    # Role methods: abilities.pgy types `self` as a subject declared in
    # another module, so it compiles as part of the whole program.
    "examples/space_station/main.pgy"
)
NON_CFG_CORPUS_MIR="$WORK_DIR/non_cfg_corpus_mir.txt"
for corpus_rel in "${NON_CFG_CORPUS[@]}"; do
    corpus_src="$ROOT_DIR/$corpus_rel"
    if [[ ! -f "$corpus_src" ]]; then
        echo "non-cfg fallback corpus member missing: $corpus_rel" >&2
        exit 1
    fi
    corpus_src_for_pgy="$(to_native_path_for_pgy "$corpus_src")"
    "$PGY" "$corpus_src_for_pgy" --native-pipeline --mir > "$NON_CFG_CORPUS_MIR"
    if ! grep -Fq "noncfg-fallbacks: total=0 routines=0 recorded=yes" \
        "$NON_CFG_CORPUS_MIR"; then
        echo "non-cfg fallback regression in $corpus_rel:" >&2
        grep -F "noncfg-fallbacks:" "$NON_CFG_CORPUS_MIR" >&2 || true
        exit 1
    fi
done
echo "non-cfg fallback corpus invariant: PASS (${#NON_CFG_CORPUS[@]} programs, all routine kinds)"

echo "cfg-body-dataflow smoke: PASS $EXAMPLE"

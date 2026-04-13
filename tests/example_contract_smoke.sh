#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEFAULT_PGY="$ROOT_DIR/bin/pgy"
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
TMP_PGY="${TMP_BASE%/}/pgy-PergyraLang-bin/pgy"
if [[ -x "${DEFAULT_PGY}.exe" ]]; then
    DEFAULT_PGY="${DEFAULT_PGY}.exe"
fi
if [[ -x "${TMP_PGY}.exe" ]]; then
    TMP_PGY="${TMP_PGY}.exe"
fi
if [[ -n "${PGY_BIN:-}" ]]; then
    PGY="$PGY_BIN"
elif [[ -x "$TMP_PGY" && ( ! -x "$DEFAULT_PGY" || "$TMP_PGY" -nt "$DEFAULT_PGY" ) ]]; then
    PGY="$TMP_PGY"
else
    PGY="$DEFAULT_PGY"
fi
WORK_BASE="$ROOT_DIR/.tmp/example-smoke"
mkdir -p "$WORK_BASE"
WORK_DIR="$(mktemp -d "$WORK_BASE.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

if [[ ! -x "$PGY" ]]; then
    echo "missing compiler binary: $PGY" >&2
    exit 1
fi

BACKENDS="${PGY_EXAMPLE_BACKENDS:-c}"
PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    fi
fi

normalize_output() {
    tr -d '\r' | sed \
        -e '/^0 error(s), 0 warning(s)$/d' \
        -e '/^pgy: compiled/d' \
        -e '/^pgy: compiled (LLVM)/d' \
        -e '/^--- output ---$/d' \
        -e '/^--- end ---$/d' | awk 'seen || length($0) > 0 { print; seen = 1 }'
}

normalize_text_file() {
    local input="$1"
    tr -d '\r' < "$input"
}

files_equal() {
    local left="$1"
    local right="$2"

    if command -v cmp >/dev/null 2>&1; then
        cmp -s "$left" "$right"
        return $?
    fi

    if command -v git >/dev/null 2>&1; then
        git diff --no-index --quiet -- "$left" "$right"
        return $?
    fi

    if [[ -n "$PYTHON_BIN" ]]; then
        "$PYTHON_BIN" - "$left" "$right" <<'PY'
import pathlib, sys
left = pathlib.Path(sys.argv[1]).read_bytes()
right = pathlib.Path(sys.argv[2]).read_bytes()
raise SystemExit(0 if left == right else 1)
PY
        return $?
    fi

    [[ "$(cat "$left")" == "$(cat "$right")" ]]
}

show_diff() {
    local left="$1"
    local right="$2"

    if command -v diff >/dev/null 2>&1; then
        diff -u "$left" "$right" || true
        return 0
    fi

    if command -v git >/dev/null 2>&1; then
        git --no-pager diff --no-index --no-prefix -- "$left" "$right" || true
        return 0
    fi

    if [[ -n "$PYTHON_BIN" ]]; then
        "$PYTHON_BIN" - "$left" "$right" <<'PY'
import difflib, pathlib, sys
left_path = pathlib.Path(sys.argv[1])
right_path = pathlib.Path(sys.argv[2])
left = left_path.read_text(encoding="utf-8", errors="replace").splitlines(True)
right = right_path.read_text(encoding="utf-8", errors="replace").splitlines(True)
sys.stdout.writelines(difflib.unified_diff(left, right, fromfile=str(left_path), tofile=str(right_path)))
PY
        return 0
    fi

    echo "--- expected ---"
    cat "$left"
    echo "--- actual ---"
    cat "$right"
}

pick_expected_file() {
    local base="$1"
    local backend="$2"

    if [[ -f "${base}.${backend}.txt" ]]; then
        printf '%s.%s.txt' "$base" "$backend"
        return 0
    fi
    if [[ -f "${base}.txt" ]]; then
        printf '%s.txt' "$base"
        return 0
    fi
    return 1
}

run_exact_output_if_present() {
    local name="$1"
    local backend="$2"
    local output="$3"
    local expected
    local cleaned_output
    local cleaned_expected
    local expected_file
    local output_file

    if ! expected="$(pick_expected_file "$ROOT_DIR/examples/$name/expected_stdout" "$backend")"; then
        return 1
    fi

    cleaned_output="$(printf '%s' "$output" | normalize_output)"
    cleaned_expected="$(cat "$expected" | normalize_output)"
    expected_file="$(mktemp "$WORK_DIR/${name}_${backend}_expected.XXXXXX")"
    output_file="$(mktemp "$WORK_DIR/${name}_${backend}_actual.XXXXXX")"
    printf '%s' "$cleaned_expected" > "$expected_file"
    printf '%s' "$cleaned_output" > "$output_file"
    if ! files_equal "$expected_file" "$output_file"; then
        echo "[example-smoke] $name backend=$backend stdout mismatch" >&2
        show_diff "$expected_file" "$output_file" >&2
        exit 1
    fi
    echo "[example-smoke] $name backend=$backend stdout exact ok"
    return 0
}

run_expect_lines() {
    local name="$1"
    local backend="$2"
    local file="$3"
    shift 3
    local output
    local out_bin="$WORK_DIR/${name}_${backend}"

    if [[ -d "$file" ]]; then
        file="$file/main.pgy"
    fi
    if [[ ! -f "$file" ]]; then
        echo "[example-smoke] $name backend=$backend missing entry: $file" >&2
        exit 1
    fi

    output="$("$PGY" "$file" --run --backend="$backend" -o "$out_bin" 2>&1)"
    if run_exact_output_if_present "$name" "$backend" "$output"; then
        return 0
    fi
    for expected in "$@"; do
        if ! grep -Fq "$expected" <<<"$output"; then
            echo "[example-smoke] $name backend=$backend missing '$expected'" >&2
            echo "--- output ---" >&2
            echo "$output" >&2
            echo "--------------" >&2
            exit 1
        fi
    done
    echo "[example-smoke] $name backend=$backend ok"
}

run_expect_file_lines() {
    local name="$1"
    local backend="$2"
    local file="$3"
    shift 3
    local content
    local expected

    expected=""
    if [[ "$file" == "$ROOT_DIR/examples/"*"/results.txt" ]]; then
        local example_dir
        example_dir="$(dirname "$file")"
        if expected="$(pick_expected_file "$example_dir/expected_results" "$backend" 2>/dev/null)"; then
            :
        else
            expected=""
        fi
    fi

    if [[ ! -f "$file" ]]; then
        echo "[example-smoke] $name missing output file $file" >&2
        exit 1
    fi
    content="$(cat "$file")"
    if [[ -n "$expected" ]]; then
        local expected_file
        local actual_file
        expected_file="$(mktemp "$WORK_DIR/${name}_${backend}_expected_file.XXXXXX")"
        actual_file="$(mktemp "$WORK_DIR/${name}_${backend}_actual_file.XXXXXX")"
        normalize_text_file "$expected" > "$expected_file"
        normalize_text_file "$file" > "$actual_file"
        if ! files_equal "$expected_file" "$actual_file"; then
            echo "[example-smoke] $name file mismatch" >&2
            show_diff "$expected_file" "$actual_file" >&2
            exit 1
        fi
        echo "[example-smoke] $name file exact ok"
        return 0
    fi
    for expected in "$@"; do
        if ! grep -Fq "$expected" <<<"$content"; then
            echo "[example-smoke] $name file missing '$expected'" >&2
            echo "--- file ---" >&2
            echo "$content" >&2
            echo "------------" >&2
            exit 1
        fi
    done
    echo "[example-smoke] $name file ok"
}

run_stable_examples() {
    local backend="$1"
    run_expect_lines "beta_resource_slots" "$backend" \
        "$ROOT_DIR/examples/beta_resource_slots.pgy" "42" "7" "3"
    run_expect_lines "beta_modules_generics" "$backend" \
        "$ROOT_DIR/examples/beta_modules_generics.pgy" "7"
    run_expect_lines "battle_simulator" "$backend" \
        "$ROOT_DIR/examples/battle_simulator" "BATTLE" "Hero" "Slime" "TOURNAMENT" "14" "true"
    run_expect_lines "biome_simulator" "$backend" \
        "$ROOT_DIR/examples/biome_simulator" "BIOME" "Red Deer" "Grey Wolf" "[World] Total migration pressure" "Day 6" "SAVING REPORT"
    run_expect_lines "fsm_factory" "$backend" \
        "$ROOT_DIR/examples/fsm_factory" "PERGYRA FACTORY FSM" "FACTORY SHIFT 1" "ALPHA CELL" "BETA CELL" "SAVING FSM REPORT"
    run_expect_lines "raid_graph_fsm" "$backend" \
        "$ROOT_DIR/examples/raid_graph_fsm" "RAID GRAPH + FSM" "RAID TURN 1" "[Raider] Iris" "[Room] VAULT" "SAVING RAID REPORT"
    run_expect_lines "campaign_graph_fsm" "$backend" \
        "$ROOT_DIR/examples/campaign_graph_fsm" "PERGYRA CAMPAIGN GRAPH + FSM" "CAMPAIGN DAY 1" "[Watch] view Iris" "FINAL CAMPAIGN SNAPSHOT" "saving examples/campaign_graph_fsm/results.txt"
    run_expect_lines "dnd_tavern_campaign" "$backend" \
        "$ROOT_DIR/examples/dnd_tavern_campaign" "=== DND TAVERN CAMPAIGN ===" "== TAVERN NIGHT ==" "== FLOOR 3 ==" "== DRAGON LAIR ==" "saving examples/dnd_tavern_campaign/results.txt"
    run_expect_lines "shopping_mall_checkout_refund" "$backend" \
        "$ROOT_DIR/examples/shopping_mall_checkout_refund" "=== PERGYRA SHOPPING CHECKOUT + REFUND ===" "[JS] mount /cart" "[API] POST /api/intents/CheckoutPurchase" "[API] POST /api/intents/RefundPurchase" "saving examples/shopping_mall_checkout_refund/results.txt"
    run_expect_lines "logistics_intent_probe" "$backend" \
        "$ROOT_DIR/examples/logistics_intent_probe" "=== PERGYRA LOGISTICS INTENT PROBE ===" "merge.true=12" "[Intent] RouteCargo ok=true" "[transfer] courier: LoadingZone.courier -> DeliveryZone.courier" "saving examples/logistics_intent_probe/results.txt"
    run_expect_lines "composite_intent_orchestration" "$backend" \
        "$ROOT_DIR/examples/composite_intent_orchestration" "[Intent] ProcessOrder=true" "[CanonicalClerk] reserved=1 charged=1 shipped=1" "[Step] fulfill phase=ok participant= ok=true"
    run_expect_lines "resource_scheduler_async_probe" "$backend" \
        "$ROOT_DIR/examples/resource_scheduler_async_probe" "=== ASYNC RESOURCE SCHEDULER PROBE ===" "[Dispatch] laneA=3 laneA=5 laneB=7 laneB=11" "[Remote] 103 105 207 211" "[Score] 144 147 240 245 total=776" "saving examples/resource_scheduler_async_probe/results.txt"
    run_expect_lines "spray_device_probe" "$backend" \
        "$ROOT_DIR/examples/spray_device_probe" "=== SPRAY DEVICE PROBE ===" "[spray] success=3 all=true" "[device] 910 911 912" "[capability] spray-batch=true device-readback=true real-gpu-backend=false"
    run_expect_lines "calendar_working" "$backend" \
        "$ROOT_DIR/examples/calendar_working/main.pgy" "total events: 3" "== 2026-4-5 ==" "Team Sync" "Dentist"
    run_expect_lines "event_closure_probe" "$backend" \
        "$ROOT_DIR/examples/event_closure_probe.pgy" "event.score=42" "event.closure=ok"
    run_expect_lines "collections_closure_probe" "$backend" \
        "$ROOT_DIR/examples/collections_closure_probe.pgy" "list.sum=7" "set.has3=true" "map.sum=70" "sizes=2/2/2"
    run_expect_lines "finance_ledger_probe" "$backend" \
        "$ROOT_DIR/examples/finance_ledger_probe/main.pgy" "=== FINANCE LEDGER PROBE ===" "[ledger]" "balanced=true" "version=wallet:min@1" "idempotency=payment:checkout-42#7"
    run_expect_lines "compliance_obligation_probe" "$backend" \
        "$ROOT_DIR/examples/compliance_obligation_probe/main.pgy" "=== COMPLIANCE OBLIGATION PROBE ===" "[obligation] KYC_REVIEW" "before.violation=false" "after.violation=true" "[violation] KYC_REVIEW"
    run_expect_lines "iot_device_adapter_probe" "$backend" \
        "$ROOT_DIR/examples/iot_device_adapter_probe/main.pgy" "=== IOT DEVICE ADAPTER PROBE ===" "[timer] poll:temp" "expired=true" "[command] target=temp" "[device] source=temp"
    run_expect_lines "subject_object_tobject" "$backend" \
        "$ROOT_DIR/examples/subject_object_tobject.pgy" "Alice" "100" "5"
    run_expect_lines "adapter_policy_stack" "$backend" \
        "$ROOT_DIR/examples/adapter_policy_stack/main.pgy" "=== ADAPTER POLICY STACK ===" "[Route] checkout -> /api/checkout" "[API] /api/checkout ok=true handle=4101" "[API] /api/refund#8831:true"
    run_expect_lines "pattern_library_basics" "$backend" \
        "$ROOT_DIR/examples/pattern_library_basics" "PERGYRA PATTERN LIBRARY BASICS" "CONTEXTUAL SINGLETON" "FACTORY / SPEC BUILDER" "STRATEGY CARD + RESOLVER" "EXPLICIT RELAY / OBSERVER"
    run_expect_lines "function_clause_order_minimal" "$backend" \
        "$ROOT_DIR/examples/function_clause_order_minimal.pgy" "clause-order-minimal"
    run_expect_lines "generic_ability_requires_minimal" "$backend" \
        "$ROOT_DIR/examples/generic_ability_requires_minimal.pgy" "generic-ability-requires-minimal"
    run_expect_lines "action_contract_inheritance_minimal" "$backend" \
        "$ROOT_DIR/examples/action_contract_inheritance_minimal.pgy" "action contract reuse minimal"
    run_expect_lines "intent_contract_derivation_minimal" "$backend" \
        "$ROOT_DIR/examples/intent_contract_derivation_minimal.pgy" "intent contract derivation minimal"
    run_expect_lines "intent_contract_pair_minimal" "$backend" \
        "$ROOT_DIR/examples/intent_contract_pair_minimal.pgy" "intent-contract-pair-minimal"
    run_expect_lines "authority_contract_pair_minimal" "$backend" \
        "$ROOT_DIR/examples/authority_contract_pair_minimal.pgy" "authority-contract-pair-minimal"
    run_expect_lines "transfer_contract_pair_minimal" "$backend" \
        "$ROOT_DIR/examples/transfer_contract_pair_minimal.pgy" "transfer-contract-pair-minimal"
    run_expect_lines "transfer_move_minimal" "$backend" \
        "$ROOT_DIR/examples/transfer_move_minimal.pgy" "transfer move minimal"
    run_expect_lines "transfer_move_typed_minimal" "$backend" \
        "$ROOT_DIR/examples/transfer_move_typed_minimal.pgy" "transfer move typed minimal"
    run_expect_lines "surface_compression_maximal" "$backend" \
        "$ROOT_DIR/examples/surface_compression_maximal.pgy" "surface compression maximal"
    run_expect_lines "zone_context_minimal" "$backend" \
        "$ROOT_DIR/examples/zone_context_minimal.pgy" "zone context minimal"
    run_expect_lines "projection_bind_group_minimal" "$backend" \
        "$ROOT_DIR/examples/projection_bind_group_minimal.pgy" "projection bind group minimal"
    run_expect_lines "projection_refresh_publish_group_minimal" "$backend" \
        "$ROOT_DIR/examples/projection_refresh_publish_group_minimal.pgy" "projection refresh publish group minimal"
    run_expect_lines "calendar_manage_event_explicit" "$backend" \
        "$ROOT_DIR/examples/calendar_manage_event_explicit.pgy" "calendar-manage-explicit" "mode=explicit" "steps=3"
    run_expect_lines "calendar_manage_event_compressed" "$backend" \
        "$ROOT_DIR/examples/calendar_manage_event_compressed.pgy" "calendar-manage-compressed" "mode=compressed" "steps=3"
    run_expect_lines "composite_intent_orchestration_explicit" "$backend" \
        "$ROOT_DIR/examples/composite_intent_orchestration_explicit.pgy" "composite-orchestration-explicit" "mode=explicit" "ok=true"
    run_expect_lines "composite_intent_orchestration_compressed" "$backend" \
        "$ROOT_DIR/examples/composite_intent_orchestration_compressed.pgy" "composite-orchestration-compressed" "mode=compressed" "ok=true"
    run_expect_lines "six_item_alignment_demo" "$backend" \
        "$ROOT_DIR/examples/six_item_alignment_demo.pgy" "six item alignment demo" "Mina" "1" "paid"
    run_expect_lines "ownership_forwarding_probe" "$backend" \
        "$ROOT_DIR/examples/ownership_forwarding_probe" "ownership forwarding probe" "inner-ref" "middle-ref" "plain-consumed" "secure-consumed" "secure-relay" "done"
    run_expect_lines "order_analytics" "$backend" \
        "$ROOT_DIR/examples/order_analytics" \
        "=== Order Analytics ===" \
        "batch: 2026-Q1-orders, size: 6" \
        "eda intent ok=true" \
        "total: 6" \
        "valid: 4" \
        "refunds: 2" \
        "revenue: 72100" \
        "avg: 18025" \
        "[ETL] loaded 4 / skipped 2 → clean_orders" \
        "etl intent ok=true" \
        "loaded: 4" \
        "skipped: 2" \
        "target: clean_orders"
    run_expect_file_lines "battle_simulator" \
        "$backend" "$ROOT_DIR/examples/battle_simulator/results.txt" "TOURNAMENT" "Hero" "Knight" "projection_ready"
    run_expect_file_lines "biome_simulator" \
        "$backend" "$ROOT_DIR/examples/biome_simulator/results.txt" "BIOME SIMULATION FINAL REPORT" "Red Deer" "Lynx" "migration:"
    run_expect_file_lines "fsm_factory" \
        "$backend" "$ROOT_DIR/examples/fsm_factory/results.txt" "FACTORY FSM REPORT" "ALPHA" "BETA" "projection_ready=true"
    run_expect_file_lines "raid_graph_fsm" \
        "$backend" "$ROOT_DIR/examples/raid_graph_fsm/results.txt" "RAID GRAPH FSM REPORT" "FORGE" "SANCTUM" "Iris relics="
    run_expect_file_lines "campaign_graph_fsm" \
        "$backend" "$ROOT_DIR/examples/campaign_graph_fsm/results.txt" "CAMPAIGN GRAPH FSM REPORT" "WATCH" "SUMMIT" "allViewsReady=true"
    run_expect_file_lines "dnd_tavern_campaign" \
        "$backend" "$ROOT_DIR/examples/dnd_tavern_campaign/results.txt" "DND TAVERN CAMPAIGN REPORT" "dragonHp=0 victory=1" "Ari [Vanguard]" "Sol [Mage]"
    run_expect_file_lines "shopping_mall_checkout_refund" \
        "$backend" "$ROOT_DIR/examples/shopping_mall_checkout_refund/results.txt" "SHOPPING CHECKOUT + REFUND REPORT" "CheckoutPurchase ok=1" "RefundPurchase ok=1" "SyncAccountProfile ok=1"
    run_expect_file_lines "logistics_intent_probe" \
        "$backend" "$ROOT_DIR/examples/logistics_intent_probe/results.txt" "PERGYRA LOGISTICS INTENT PROBE" "RouteCargo ok=true" "delivery=received" "seal=Ivo seal qty=8"
    run_expect_file_lines "resource_scheduler_async_probe" \
        "$backend" "$ROOT_DIR/examples/resource_scheduler_async_probe/results.txt" "ASYNC RESOURCE SCHEDULER PROBE" "budget.remaining=14 reserved=26" "[Remote] 103 105 207 211" "total=776"
    run_expect_file_lines "spray_device_probe" \
        "$backend" "$ROOT_DIR/examples/spray_device_probe/results.txt" "SPRAY DEVICE PROBE" "[spray] success=3 all=true" "[device] 910 911 912"
    run_expect_file_lines "adapter_policy_stack" \
        "$backend" "$ROOT_DIR/examples/adapter_policy_stack/results.txt" "ADAPTER POLICY STACK" "/api/checkout" "/api/refund#8831:true"
}

run_qubit_example() {
    local backend="$1"
    local output
    local values
    local line1
    local line2
    local line3
    local line4
    local line5
    local out_bin="$WORK_DIR/beta_qubit_experimental_${backend}"

    output="$("$PGY" "$ROOT_DIR/examples/beta_qubit_experimental.pgy" --run --backend="$backend" -o "$out_bin" 2>&1)"
    values="$(grep -E '^(0|1|2)$' <<<"$output" || true)"

    line1="$(sed -n '1p' <<<"$values")"
    line2="$(sed -n '2p' <<<"$values")"
    line3="$(sed -n '3p' <<<"$values")"
    line4="$(sed -n '4p' <<<"$values")"
    line5="$(sed -n '5p' <<<"$values")"

    if [[ "$line1" != "2" ]]; then
        echo "[example-smoke] beta_qubit_experimental backend=$backend missing initial superposition state" >&2
        echo "$output" >&2
        exit 1
    fi

    if [[ -z "$line2" || "$line2" != "$line3" ]]; then
        echo "[example-smoke] beta_qubit_experimental backend=$backend repeated measurement mismatch" >&2
        echo "$output" >&2
        exit 1
    fi

    if [[ -z "$line4" || "$line4" != "$line5" ]]; then
        echo "[example-smoke] beta_qubit_experimental backend=$backend entangled pair mismatch" >&2
        echo "$output" >&2
        exit 1
    fi

    echo "[example-smoke] beta_qubit_experimental backend=$backend ok"
}

for backend in $BACKENDS; do
    run_stable_examples "$backend"
done

for backend in $BACKENDS; do
    run_qubit_example "$backend"
done

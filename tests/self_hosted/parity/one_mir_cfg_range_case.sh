# Sourced after the while rung; reuses one admission and gate owner.

SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/forloop.pgy"
RANGE_FACT_OWNER="$ROOT_DIR/src/self_hosted/air/mir_range_cfg_certificate_fact_owner.pgy"
RANGE_SHAPE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_range_cfg_shape_owner.pgy"
RANGE_PLAN_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_range_cfg_plan_fact_owner.pgy"
RANGE_EMISSION_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_range_cfg_emission_owner.pgy"
for owner in "$RANGE_FACT_OWNER" "$RANGE_SHAPE_OWNER" \
    "$RANGE_PLAN_OWNER" "$RANGE_EMISSION_OWNER"; do
    require_file "$owner"
done
for term in DirectMirRangeCfgCertificateFactFromIndex \
    DirectMirRangeCfgCertificateFactMutationRejected; do
    grep -Fq -- "$term" "$RANGE_FACT_OWNER" ||
        fail "range AIR fact lacks $term"
done
grep -Fq -- 'DirectMirRangeCfgShapeFactFromOwners' "$RANGE_SHAPE_OWNER" ||
    fail "range target-neutral shape projection is missing"
grep -Fq -- 'DirectMirRangeCfgPlanFactMutationRejected' "$RANGE_PLAN_OWNER" ||
    fail "range fixed-plan mutation ratchet is missing"
grep -Fq -- 'DirectMirRangeCfgEmitC' "$RANGE_EMISSION_OWNER" &&
    grep -Fq -- 'DirectMirRangeCfgEmitLlvm' "$RANGE_EMISSION_OWNER" ||
    fail "range emission responsibility must retain both backends"

run_range_projected_and_compare() {
    local expected="$1" target actual
    for target in c llvm; do
        (cd "$ROOT_DIR" && "$WORK_DIR/ifelse.one.$target.exe") |
            pgy_selfhost_normalize_text_artifact >"$WORK_DIR/range-$target.run"
        actual="$(cat "$WORK_DIR/range-$target.run")"
        [[ "$actual" == "$expected" ]] ||
            fail "$target range projection produced '$actual', expected '$expected'"
    done
}

require_file "$SOURCE"; produce_one_mir; mir_digest="$(hash_file "$MIR_ARTIFACT")"
[[ "$(wc -c <"$MIR_ARTIFACT" | tr -d ' ')" == 3197 && "$mir_digest" == \
    02a683a087535bb5cd66031da03994b8c7a3b02012fdb825ea0722d35b161720 && \
    "$(grep -o '"id":[0-9]*,"reachable":true' "$MIR_ARTIFACT" | wc -l | tr -d ' ')" == 4 && \
    "$(grep -o '"kind":"phi"' "$MIR_ARTIFACT" | wc -l | tr -d ' ')" == 0 ]] ||
    fail "forloop producer identity or four-block/no-phi shape drifted"
project_one_target c "$C_ARTIFACT" "$mir_digest"
project_one_target llvm "$LLVM_ARTIFACT" "$mir_digest"
for fact in 'long long pgy_local_0 = 0;' 'while (pgy_local_0 < 3)' \
    'printf("%lld\n", (long long)pgy_local_0);' \
    'pgy_local_0 = pgy_local_0 + 1;' 'return 0;'; do
    grep -Fq -- "$fact" "$C_ARTIFACT" || fail "range C fact drifted: $fact"
done
[[ "$(grep -o 'while (' "$C_ARTIFACT" | wc -l | tr -d ' ')" == 1 ]] &&
    ! grep -Fq 'goto' "$C_ARTIFACT" || fail "range C topology drifted"
for fact in '[6 x i8] c"%lld\0A\00"' \
    'declare i32 @printf(ptr, ...)' \
    '%pgy.local.slot = alloca i64, align 8' \
    'store i64 0, ptr %pgy.local.slot, align 8' \
    '%pgy.local.current = load i64, ptr %pgy.local.slot, align 8' \
    '%pgy.condition.range = icmp slt i64 %pgy.local.current, 3' \
    'br i1 %pgy.condition.range, label %pgy.block.2, label %pgy.block.3' \
    '%pgy.logged.range = call i32 (ptr, ...) @printf(ptr @.pgy.int.line.format, i64 %pgy.local.current)' \
    '%pgy.local.next = add i64 %pgy.local.current, 1' \
    'store i64 %pgy.local.next, ptr %pgy.local.slot, align 8' 'ret i32 0'; do
    grep -Fq -- "$fact" "$LLVM_ARTIFACT" || fail "range LLVM fact drifted: $fact"
done
[[ "$(grep -F ' alloca i64' "$LLVM_ARTIFACT" | wc -l | tr -d ' ')" == 1 &&
    "$(grep -F ' load i64' "$LLVM_ARTIFACT" | wc -l | tr -d ' ')" == 1 &&
    "$(grep -F ' store i64' "$LLVM_ARTIFACT" | wc -l | tr -d ' ')" == 2 &&
    "$(grep -F 'br label %pgy.block.1' "$LLVM_ARTIFACT" | wc -l | tr -d ' ')" == 2 ]] &&
    ! grep -Fq ' phi ' "$LLVM_ARTIFACT" || fail "range LLVM topology drifted"
compile_artifacts; run_and_compare $'0\n1\n2'
assert_mir_identity "$mir_digest" "forloop backend executions"

# The emitter must derive the bounds from the one admitted range facts rather
# than from this fixture. Mutate every scalar and graph occurrence together.
baseline_mir="$MIR_ARTIFACT"; baseline_c="$C_ARTIFACT"; baseline_llvm="$LLVM_ARTIFACT"
generalized="$(make_mutation generalized_range \
    's/"expr0":"0"/"expr0":"2"/g;s/"text":"0"/"text":"2"/g;s/"expr1":"3"/"expr1":"5"/g;s/"text":"3"/"text":"5"/g' \
    '"text":"5"')"
MIR_ARTIFACT="$generalized"
C_ARTIFACT="$WORK_DIR/forloop.generalized.c"
LLVM_ARTIFACT="$WORK_DIR/forloop.generalized.ll"
generalized_digest="$(hash_file "$MIR_ARTIFACT")"
project_one_target c "$C_ARTIFACT" "$generalized_digest"
project_one_target llvm "$LLVM_ARTIFACT" "$generalized_digest"
compile_artifacts; run_range_projected_and_compare $'2\n3\n4'
assert_mir_identity "$generalized_digest" "generalized range backend executions"
MIR_ARTIFACT="$baseline_mir"; C_ARTIFACT="$baseline_c"; LLVM_ARTIFACT="$baseline_llvm"

zero_trip="$(make_mutation zero_trip_range \
    's/"expr0":"0"/"expr0":"3"/g;s/"text":"0"/"text":"3"/g' \
    '"expr0":"3"')"
MIR_ARTIFACT="$zero_trip"
C_ARTIFACT="$WORK_DIR/forloop.zero-trip.c"
LLVM_ARTIFACT="$WORK_DIR/forloop.zero-trip.ll"
zero_trip_digest="$(hash_file "$MIR_ARTIFACT")"
project_one_target c "$C_ARTIFACT" "$zero_trip_digest"
project_one_target llvm "$LLVM_ARTIFACT" "$zero_trip_digest"
compile_artifacts; run_range_projected_and_compare ''
assert_mir_identity "$zero_trip_digest" "zero-trip range backend executions"
MIR_ARTIFACT="$baseline_mir"; C_ARTIFACT="$baseline_c"; LLVM_ARTIFACT="$baseline_llvm"

mutation="$(make_mutation range_summary_kind \
    's/"kind":"for"/"kind":"while"/' '"kind":"while"')"
expect_rejected_without_artifact range_summary_kind "$mutation" 'range|loop|summary|CFG'
mutation="$(make_mutation range_summary_flags \
    's/"flags":1/"flags":0/' '"flags":0')"
expect_rejected_without_artifact range_summary_flags "$mutation" 'range|loop|summary|CFG'
mutation="$(make_mutation range_iteration_count \
    's/"iteration_type_fact_count":1/"iteration_type_fact_count":0/' \
    '"iteration_type_fact_count":0')"
expect_rejected_without_artifact range_iteration_count "$mutation" \
    'range|iteration|fact|count|CFG'
mutation="$(make_mutation range_iteration_identity \
    's/"iteration_syntax_id":6/"iteration_syntax_id":7/' \
    '"iteration_syntax_id":7')"
expect_rejected_without_artifact range_iteration_identity "$mutation" \
    'range|iteration|identity|syntax|CFG'
mutation="$(make_mutation range_iteration_type \
    's/"binding_type":"Int"/"binding_type":"String"/' \
    '"binding_type":"String"')"
expect_rejected_without_artifact range_iteration_type "$mutation" \
    'range|iteration|type|Int'
mutation="$(make_mutation range_iteration_hoist \
    's/"collection_hoisted":false/"collection_hoisted":true/' \
    '"collection_hoisted":true')"
expect_rejected_without_artifact range_iteration_hoist "$mutation" \
    'range|iteration|collection|hoist'
mutation="$(make_mutation range_missing_backedge \
    's/],"succ_true":1},{"id":3/],"succ_true":3},{"id":3/' \
    '],"succ_true":3},{"id":3')"
expect_rejected_without_artifact range_missing_backedge "$mutation" \
    'range|backedge|CFG|successor'
mutation="$(make_mutation range_wrong_exit \
    's/,"succ_true":2,"succ_false":3/,"succ_true":2,"succ_false":0/' \
    '"succ_true":2,"succ_false":0')"
expect_rejected_without_artifact range_wrong_exit "$mutation" \
    'range|exit|CFG|successor'
mutation="$(make_mutation range_start_graph \
    's/"text":"0"/"text":"2"/' '"text":"2"')"
expect_rejected_without_artifact range_start_graph "$mutation" \
    'range|start|graph|literal'
mutation="$(make_mutation range_stop_graph \
    's/"text":"3"/"text":"4"/' '"text":"4"')"
expect_rejected_without_artifact range_stop_graph "$mutation" \
    'range|stop|graph|literal'
mutation="$(make_mutation range_raw_cross_binding \
    's/"expr0":"0"/"expr0":"2"/2' '"expr0":"2"')"
expect_rejected_without_artifact range_raw_cross_binding "$mutation" \
    'range|start|raw|scalar|binding'
mutation="$(make_mutation range_binding \
    's/"arg0":"i"/"arg0":"j"/2' '"arg0":"j"')"
expect_rejected_without_artifact range_binding "$mutation" \
    'range|local|binding|branch'
mutation="$(make_mutation range_log_target \
    's/"call_target_name":"ToString"/"call_target_name":"ToInt"/' \
    '"call_target_name":"ToInt"')"
expect_rejected_without_artifact range_log_target "$mutation" \
    'range|Log|ToString|target|graph'
mutation="$(make_mutation range_unexpected_use \
    's/"uses":\[\]/"uses":["i.1"]/' '"uses":["i.1"]')"
expect_rejected_without_artifact range_unexpected_use "$mutation" \
    'range|use|SSA|CFG'
mutation="$(make_mutation range_invented_phi \
    's/"kind":"stmt","name":"stmt"/"kind":"phi","name":"stmt"/' \
    '"kind":"phi","name":"stmt"')"
expect_rejected_without_artifact range_invented_phi "$mutation" \
    'range|phi|CFG|instruction'

echo "[$LABEL] ifelse + if_else_assign + reassign_block + nestedif + whileloop + forloop one-MIR CFG/AIR-plan gate ok (sha256=$mir_digest)"

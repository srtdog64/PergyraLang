# Sourced after the nested rung; reuses one admission and gate owner.

SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/whileloop.pgy"
LOOP_FACT_OWNER="$ROOT_DIR/src/self_hosted/air/mir_loop_cfg_certificate_fact_owner.pgy"
LOOP_SHAPE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_loop_cfg_shape_owner.pgy"
LOOP_PLAN_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_loop_cfg_plan_fact_owner.pgy"
LOOP_EMISSION_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_loop_cfg_emission_owner.pgy"
for owner in "$LOOP_FACT_OWNER" "$LOOP_SHAPE_OWNER" \
    "$LOOP_PLAN_OWNER" "$LOOP_EMISSION_OWNER"; do
    require_file "$owner"
done
for term in DirectMirLoopCfgCertificateFactFromIndex \
    DirectMirLoopCfgCertificateFactMutationRejected; do
    grep -Fq -- "$term" "$LOOP_FACT_OWNER" || fail "loop AIR fact lacks $term"
done
grep -Fq -- 'DirectMirLoopCfgShapeFactFromOwners' "$LOOP_SHAPE_OWNER" ||
    fail "loop target-neutral shape projection is missing"
grep -Fq -- 'DirectMirLoopCfgPlanFactMutationRejected' "$LOOP_PLAN_OWNER" ||
    fail "loop fixed-plan mutation ratchet is missing"
grep -Fq -- 'DirectMirLoopCfgEmitC' "$LOOP_EMISSION_OWNER" &&
    grep -Fq -- 'DirectMirLoopCfgEmitLlvm' "$LOOP_EMISSION_OWNER" ||
    fail "loop emission responsibility must retain both backends"

require_file "$SOURCE"; produce_one_mir; mir_digest="$(hash_file "$MIR_ARTIFACT")"
[[ "$(wc -c <"$MIR_ARTIFACT" | tr -d ' ')" == 4692 && "$mir_digest" == \
    c48c9f598969a01864371bac9f11609ccfaecf499444eb5e263eed8a57e50fb0 && \
    "$(grep -o '"id":[0-9]*,"reachable":true' "$MIR_ARTIFACT" | wc -l | tr -d ' ')" == 4 && \
    "$(grep -o '"kind":"phi"' "$MIR_ARTIFACT" | wc -l | tr -d ' ')" == 1 ]] ||
    fail "whileloop producer identity or four-block/one-phi shape drifted"
project_one_target c "$C_ARTIFACT" "$mir_digest"
project_one_target llvm "$LLVM_ARTIFACT" "$mir_digest"
for fact in 'long long pgy_local_0 = 0;' 'while (pgy_local_0 < 3)' \
    'printf("%lld\n", (long long)pgy_local_0);' \
    'pgy_local_0 = pgy_local_0 + 1;' 'return 0;'; do
    grep -Fq -- "$fact" "$C_ARTIFACT" || fail "loop C fact drifted: $fact"
done
[[ "$(grep -o 'while (' "$C_ARTIFACT" | wc -l | tr -d ' ')" == 1 ]] &&
    ! grep -Fq 'goto' "$C_ARTIFACT" || fail "loop C topology drifted"
for fact in '[6 x i8] c"%lld\0A\00"' \
    'declare i32 @printf(ptr, ...)' \
    'br label %pgy.block.1' \
    '%pgy.local.loop = phi i64 [ %pgy.local.entry, %entry ], [ %pgy.local.next, %pgy.block.2 ]' \
    '%pgy.condition.loop = icmp slt i64 %pgy.local.loop, 3' \
    'br i1 %pgy.condition.loop, label %pgy.block.2, label %pgy.block.3' \
    '%pgy.logged.loop = call i32 (ptr, ...) @printf(ptr @.pgy.int.line.format, i64 %pgy.local.loop)' \
    '%pgy.local.next = add i64 %pgy.local.loop, 1' 'ret i32 0'; do
    grep -Fq -- "$fact" "$LLVM_ARTIFACT" || fail "loop LLVM fact drifted: $fact"
done
[[ "$(grep -F ' phi i64 ' "$LLVM_ARTIFACT" | wc -l | tr -d ' ')" == 1 &&
    "$(grep -F 'br label %pgy.block.1' "$LLVM_ARTIFACT" | wc -l | tr -d ' ')" == 2 ]] &&
    ! grep -Eq 'alloca| load | store ' "$LLVM_ARTIFACT" ||
    fail "loop LLVM topology drifted"
compile_artifacts; run_and_compare $'0\n1\n2'
assert_mir_identity "$mir_digest" "whileloop backend executions"

# Phi use order is not predecessor identity. Reverse it and require the same
# target artifacts, proving definition blocks remain the owner.
baseline_mir="$MIR_ARTIFACT"; baseline_c="$C_ARTIFACT"; baseline_llvm="$LLVM_ARTIFACT"
permuted="$(make_mutation phi_incoming_order \
    's/"uses":\["i\.1","i\.5"\]/"uses":["i.5","i.1"]/' \
    '"uses":["i.5","i.1"]')"
MIR_ARTIFACT="$permuted"
C_ARTIFACT="$WORK_DIR/whileloop.permuted.c"
LLVM_ARTIFACT="$WORK_DIR/whileloop.permuted.ll"
permuted_digest="$(hash_file "$MIR_ARTIFACT")"
project_one_target c "$C_ARTIFACT" "$permuted_digest"
project_one_target llvm "$LLVM_ARTIFACT" "$permuted_digest"
cmp -s "$baseline_c" "$C_ARTIFACT" && cmp -s "$baseline_llvm" "$LLVM_ARTIFACT" ||
    fail "phi incoming order changed a predecessor-owned target artifact"
MIR_ARTIFACT="$baseline_mir"; C_ARTIFACT="$baseline_c"; LLVM_ARTIFACT="$baseline_llvm"

mutation="$(make_mutation loop_summary_kind \
    's/"kind":"while"/"kind":"for"/' '"kind":"for"')"
expect_rejected_without_artifact loop_summary_kind "$mutation" 'loop|summary|CFG'
mutation="$(make_mutation loop_summary_flags \
    's/"flags":1/"flags":0/' '"flags":0')"
expect_rejected_without_artifact loop_summary_flags "$mutation" 'loop|summary|CFG'
mutation="$(make_mutation loop_summary_effect \
    's/"effect_delta":0/"effect_delta":1/' '"effect_delta":1')"
expect_rejected_without_artifact loop_summary_effect "$mutation" 'loop|summary|CFG'
mutation="$(make_mutation loop_summary_state_span \
    's/"entry_state_count":0/"entry_state_count":1/' \
    '"entry_state_count":1')"
expect_rejected_without_artifact loop_summary_state_span "$mutation" \
    'loop|summary|state|CFG'
mutation="$(make_mutation missing_loop_phi \
    's/"kind":"phi"/"kind":"def"/' '"kind":"def","name":"i"')"
expect_rejected_without_artifact missing_loop_phi "$mutation" 'loop|phi|CFG'
mutation="$(make_mutation duplicate_preheader_incoming \
    's/"uses":\["i\.1","i\.5"\]/"uses":["i.1","i.1"]/' \
    '"uses":["i.1","i.1"]')"
expect_rejected_without_artifact duplicate_preheader_incoming "$mutation" \
    'loop|phi|predecessor|incoming'
mutation="$(make_mutation stale_header_use \
    's/"uses":\["i\.2"\]/"uses":["i.1"]/' '"uses":["i.1"]')"
expect_rejected_without_artifact stale_header_use "$mutation" 'loop|SSA|use|condition'
mutation="$(make_mutation stale_log_use \
    's/"uses":\["i\.2"\]/"uses":["i.1"]/2' '"uses":["i.1"]')"
expect_rejected_without_artifact stale_log_use "$mutation" 'loop|SSA|use|Log'
mutation="$(make_mutation stale_increment_use \
    's/"uses":\["i\.2"\]/"uses":["i.1"]/3' '"uses":["i.1"]')"
expect_rejected_without_artifact stale_increment_use "$mutation" 'loop|SSA|use|increment'
mutation="$(make_mutation stale_increment_result \
    's/"result":"i\.5"/"result":"i.6"/' '"result":"i.6"')"
expect_rejected_without_artifact stale_increment_result "$mutation" 'loop|phi|SSA|increment'
mutation="$(make_mutation missing_backedge \
    's/],"succ_true":1},{"id":3/],"succ_true":3},{"id":3/' \
    '],"succ_true":3},{"id":3')"
expect_rejected_without_artifact missing_backedge "$mutation" 'loop|backedge|CFG|successor'
mutation="$(make_mutation wrong_loop_exit \
    's/,"succ_true":2,"succ_false":3/,"succ_true":2,"succ_false":0/' \
    '"succ_true":2,"succ_false":0')"
expect_rejected_without_artifact wrong_loop_exit "$mutation" 'loop|exit|CFG|successor'
mutation="$(make_mutation condition_operator \
    's/"kind":"less"/"kind":"greater"/' '"kind":"greater"')"
expect_rejected_without_artifact condition_operator "$mutation" 'loop|binary|condition|graph'
mutation="$(make_mutation increment_operator \
    's/"kind":"add"/"kind":"multiply"/' '"kind":"multiply"')"
expect_rejected_without_artifact increment_operator "$mutation" 'loop|binary|increment|graph'
mutation="$(make_mutation assignment_target_graph \
    's/"expr1_graph":{"root":0,"nodes":\[{"kind":"leaf","text":"i"/"expr1_graph":{"root":0,"nodes":[{"kind":"leaf","text":"j"/' \
    '"expr1_graph":{"root":0,"nodes":[{"kind":"leaf","text":"j"')"
expect_rejected_without_artifact assignment_target_graph "$mutation" \
    'loop|assignment|target|graph'
mutation="$(make_mutation tostring_target \
    's/"call_target_name":"ToString"/"call_target_name":"ToInt"/' \
    '"call_target_name":"ToInt"')"
expect_rejected_without_artifact tostring_target "$mutation" 'loop|Log|target|graph'

source "$ROOT_DIR/tests/self_hosted/parity/one_mir_cfg_range_case.sh"

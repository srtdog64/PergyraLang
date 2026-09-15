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
[[ -s "$MIR_ARTIFACT" &&
    "$(grep -o '"id":[0-9]*,"reachable":true' "$MIR_ARTIFACT" | wc -l | tr -d ' ')" == 4 && \
    "$(grep -o '"kind":"phi"' "$MIR_ARTIFACT" | wc -l | tr -d ' ')" == 1 ]] ||
    fail "whileloop admitted MIR or four-block/one-phi shape drifted"
project_one_target c "$C_ARTIFACT" "$mir_digest"
project_one_target llvm "$LLVM_ARTIFACT" "$mir_digest"
for fact in 'pgy_local_0 = 0' 'pgy_local_0 < 3' \
    'printf("%s\n", pgy_tostr(pgy_local_0))' \
    'pgy_local_0 + 1'; do
    grep -Fq -- "$fact" "$C_ARTIFACT" || fail "loop C lost semantic fact: $fact"
done
[[ "$(grep -E 'goto pgy_[a-z0-9_]*block_1;' "$C_ARTIFACT" | wc -l | tr -d ' ')" == 2 ]] ||
    fail "loop C backedge topology drifted"
for fact in 'alloca i64' 'icmp slt i64' \
    'call i32 (ptr, ...) @printf' 'add i64' 'ret i32 0'; do
    grep -Fq -- "$fact" "$LLVM_ARTIFACT" || fail "loop LLVM lost semantic fact: $fact"
done
[[ "$(grep -F ' phi i64 ' "$LLVM_ARTIFACT" | wc -l | tr -d ' ')" == 0 &&
    "$(grep -E 'br label %pgy\.[a-z0-9.]*block\.1' "$LLVM_ARTIFACT" | wc -l | tr -d ' ')" == 2 ]] ||
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
expect_rejected_without_artifact stale_increment_use "$mutation" \
    'loop|SSA|use|increment|definition.*invalid'
mutation="$(make_mutation stale_increment_result \
    's/"result":"i\.5"/"result":"i.6"/' '"result":"i.6"')"
expect_rejected_without_artifact stale_increment_result "$mutation" 'loop|phi|SSA|increment'
mutation="$(make_mutation increment_abi \
    's/"result":"i\.5","arg0":"i","arg1":"local","slot_anchor":null,"abi_type_name":"Int"/"result":"i.5","arg0":"i","arg1":"local","slot_anchor":null,"abi_type_name":"String"/' \
    '"result":"i.5","arg0":"i","arg1":"local","slot_anchor":null,"abi_type_name":"String"')"
expect_rejected_without_artifact increment_abi "$mutation" \
    'loop|increment|assignment|ABI|type|definition.*invalid'
mutation="$(make_mutation missing_backedge \
    's/],"succ_true":1},{"id":3/],"succ_true":3},{"id":3/' \
    '],"succ_true":3},{"id":3')"
expect_rejected_without_artifact missing_backedge "$mutation" 'loop|backedge|CFG|successor'
# Redirecting the false edge to another valid block creates a different cyclic
# CFG; it is not malformed merely because it differs from this fixture.
# A different supported comparison is a different valid CFG program, not a
# malformed instance of this fixture topology. The general owner must accept it.
mutation="$(make_mutation increment_operator \
    's/"kind":"add"/"kind":"multiply"/' '"kind":"multiply"')"
expect_rejected_without_artifact increment_operator "$mutation" \
    'loop|binary|increment|graph|definition.*invalid'
mutation="$(make_mutation assignment_target_graph \
    's/"expr1_graph":{"root":0,"nodes":\[{"kind":"leaf","text":"i"/"expr1_graph":{"root":0,"nodes":[{"kind":"leaf","text":"j"/' \
    '"expr1_graph":{"root":0,"nodes":[{"kind":"leaf","text":"j"')"
expect_rejected_without_artifact assignment_target_graph "$mutation" \
    'loop|assignment|target|graph|definition.*invalid'
mutation="$(make_mutation tostring_target \
    's/"call_target_name":"ToString"/"call_target_name":"ToInt"/' \
    '"call_target_name":"ToInt"')"
expect_rejected_without_artifact tostring_target "$mutation" 'loop|Log|target|graph'

source "$ROOT_DIR/tests/self_hosted/parity/one_mir_cfg_range_case.sh"

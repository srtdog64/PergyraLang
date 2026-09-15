# Sourced after the direct-false rung; reuses one admission and gate owner.

SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/nestedif.pgy"
NESTED_FACT_OWNER="$ROOT_DIR/src/self_hosted/air/mir_nested_cfg_certificate_fact_owner.pgy"
NESTED_SHAPE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_nested_cfg_shape_owner.pgy"
NESTED_EMISSION_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_nested_cfg_emission_owner.pgy"
for owner in "$NESTED_FACT_OWNER" "$NESTED_SHAPE_OWNER" "$NESTED_EMISSION_OWNER"; do
    require_file "$owner"
done
for term in DirectMirNestedCfgCertificateFactFromIndex \
    DirectMirNestedCfgCertificateFactMutationRejected; do
    grep -Fq -- "$term" "$NESTED_FACT_OWNER" || fail "nested AIR fact lacks $term"
done
grep -Fq -- 'DirectMirNestedCfgShapeFactsFromOwners' "$NESTED_SHAPE_OWNER" ||
    fail "nested target-neutral shape projection is missing"
grep -Fq -- 'DirectMirNestedCfgEmitC' "$NESTED_EMISSION_OWNER" &&
    grep -Fq -- 'DirectMirNestedCfgEmitLlvm' "$NESTED_EMISSION_OWNER" ||
    fail "nested emission responsibility must retain both backends"

require_file "$SOURCE"; produce_one_mir; mir_digest="$(hash_file "$MIR_ARTIFACT")"
[[ -s "$MIR_ARTIFACT" &&
    "$(grep -o '"id":[0-9]*,"reachable":true' "$MIR_ARTIFACT" | wc -l | tr -d ' ')" == 5 && \
    "$(grep -o '"kind":"phi"' "$MIR_ARTIFACT" | wc -l | tr -d ' ')" == 0 ]] ||
    fail "nestedif admitted MIR or five-block/no-phi shape drifted"
project_one_target c "$C_ARTIFACT" "$mir_digest"
project_one_target llvm "$LLVM_ARTIFACT" "$mir_digest"
grep -Fq 'pgy_local_0 = 5' "$C_ARTIFACT" || fail "nested C lost value definition"
grep -Fq 'printf("%s\n", "big");' "$C_ARTIFACT" || fail "nested C lost terminal effect"
[[ "$(grep -o 'if (' "$C_ARTIFACT" | wc -l | tr -d ' ')" == 2 ]] ||
    fail "nested C lost one of two branch conditions"
[[ "$(grep -F 'icmp sgt i64' "$LLVM_ARTIFACT" | wc -l | tr -d ' ')" == 2 ]] ||
    fail "nested LLVM conditions did not preserve both comparisons"
! grep -Fq ' phi ' "$LLVM_ARTIFACT" || fail "nested LLVM projection invented phi"
[[ "$(grep -F 'br i1 ' "$LLVM_ARTIFACT" | wc -l | tr -d ' ')" == 2 ]] ||
    fail "nested LLVM lost one of two branch edges"
grep -Fq 'call i32 (ptr, ...) @printf' "$LLVM_ARTIFACT" ||
    fail "nested LLVM lost terminal effect"
compile_artifacts; run_and_compare big
assert_mir_identity "$mir_digest" "nestedif backend executions"

mutation="$(make_mutation inner_branch_identity \
    's/"id":1,"kind":"branch"/"id":1,"kind":"stmt"/' \
    '"id":1,"kind":"stmt"')"
expect_rejected_without_artifact inner_branch_identity "$mutation" \
    'nested|branch.*identity|conditional|CFG'
mutation="$(make_mutation inner_condition_use \
    's/"uses":\["x\.1"\]/"uses":["x.9"]/2' '"uses":["x.9"]')"
expect_rejected_without_artifact inner_condition_use "$mutation" \
    'nested|branch.*use|condition.*(use|invalid)|SSA|use[^[:alnum:]]+edge|direct MIR scalar CFG program expression admission is invalid: stage=leaf-operand'
mutation="$(make_mutation missing_inner_false_edge \
    's/,"succ_true":2,"succ_false":3/,"succ_true":2,"succ_false_removed":3/' \
    '"succ_false_removed":3')"
expect_rejected_without_artifact missing_inner_false_edge "$mutation" \
    'CFG|nested|successor|conditional[^[:alnum:]]+edge'
# Redirecting a well-formed edge to another valid block is not a malformed MIR
# fact. The general CFG owner must compile that graph rather than enforce this
# fixture's historical five-block topology.

source "$ROOT_DIR/tests/self_hosted/parity/one_mir_cfg_loop_case.sh"

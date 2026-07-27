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
[[ "$(wc -c <"$MIR_ARTIFACT" | tr -d ' ')" == 3687 && "$mir_digest" == \
    20e5b34b43bf7658331760cd1c5314aeb30bf8db7131686fe3fc79da8c6b3db0 && \
    "$(grep -o '"id":[0-9]*,"reachable":true' "$MIR_ARTIFACT" | wc -l | tr -d ' ')" == 5 && \
    "$(grep -o '"kind":"phi"' "$MIR_ARTIFACT" | wc -l | tr -d ' ')" == 0 ]] ||
    fail "nestedif producer identity or five-block/no-phi shape drifted"
project_one_target c "$C_ARTIFACT" "$mir_digest"
project_one_target llvm "$LLVM_ARTIFACT" "$mir_digest"
for fact in 'long long pgy_local_0 = 5;' 'if (pgy_local_0 > 0)' \
    'if (pgy_local_0 > 3)' 'printf("%s\n", "big");'; do
    grep -Fq -- "$fact" "$C_ARTIFACT" || fail "nested C fact drifted: $fact"
done
[[ "$(grep -o 'if (' "$C_ARTIFACT" | wc -l | tr -d ' ')" == 2 ]] &&
    ! grep -Fq 'else' "$C_ARTIFACT" || fail "nested C topology drifted"
[[ "$(grep -F 'icmp sgt i64 %pgy.local.entry' "$LLVM_ARTIFACT" | wc -l | tr -d ' ')" == 2 ]] ||
    fail "nested LLVM conditions do not share the entry SSA"
! grep -Fq ' phi ' "$LLVM_ARTIFACT" || fail "nested LLVM projection invented phi"
for fact in '[4 x i8] c"%s\0A\00"' '[4 x i8] c"big\00"' \
    'br i1 %pgy.condition.outer, label %pgy.block.1, label %pgy.block.4' \
    'br i1 %pgy.condition.inner, label %pgy.block.2, label %pgy.block.3'; do
    grep -Fq -- "$fact" "$LLVM_ARTIFACT" || fail "nested LLVM fact drifted: $fact"
done
[[ "$(grep -F 'br label %pgy.block.3' "$LLVM_ARTIFACT" | wc -l | tr -d ' ')" == 1 &&
    "$(grep -F 'br label %pgy.block.4' "$LLVM_ARTIFACT" | wc -l | tr -d ' ')" == 1 ]] ||
    fail "nested LLVM merge forwarding drifted"
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
    'nested|branch.*use|condition.*use|SSA|use[^[:alnum:]]+edge'
mutation="$(make_mutation missing_inner_false_edge \
    's/,"succ_true":2,"succ_false":3/,"succ_true":2,"succ_false_removed":3/' \
    '"succ_false_removed":3')"
expect_rejected_without_artifact missing_inner_false_edge "$mutation" \
    'CFG|nested|successor|conditional[^[:alnum:]]+edge'
mutation="$(make_mutation outer_false_edge \
    's/,"succ_true":1,"succ_false":4/,"succ_true":1,"succ_false":3/' \
    '"succ_true":1,"succ_false":3')"
expect_rejected_without_artifact outer_false_edge "$mutation" \
    'CFG|nested|merge|successor'
mutation="$(make_mutation inner_merge_edge \
    's/],"succ_true":3},{"id":3/],"succ_true":4},{"id":3/' \
    '],"succ_true":4},{"id":3')"
expect_rejected_without_artifact inner_merge_edge "$mutation" \
    'CFG|nested|merge|successor'
mutation="$(make_mutation outer_merge_edge \
    's/"instructions":\[\],"succ_true":4},{"id":4/"instructions":[],"succ_true":3},{"id":4/' \
    '"instructions":[],"succ_true":3},{"id":4')"
expect_rejected_without_artifact outer_merge_edge "$mutation" \
    'CFG|nested|merge|successor'

echo "[$LABEL] ifelse + if_else_assign + reassign_block + nestedif one-MIR CFG/AIR-plan gate ok (sha256=$mir_digest)"

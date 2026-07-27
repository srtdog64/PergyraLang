# Sourced after the range rung; reuses one admission and gate owner.

SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/break_after_stmt.pgy"
BREAK_FACT_OWNER="$ROOT_DIR/src/self_hosted/air/mir_break_cfg_certificate_fact_owner.pgy"
BREAK_SHAPE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_break_cfg_shape_owner.pgy"
BREAK_PLAN_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_break_cfg_plan_fact_owner.pgy"
BREAK_EMISSION_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_break_cfg_emission_owner.pgy"
for owner in "$BREAK_FACT_OWNER" "$BREAK_SHAPE_OWNER" \
    "$BREAK_PLAN_OWNER" "$BREAK_EMISSION_OWNER"; do
    require_file "$owner"
done
for term in DirectMirBreakCfgCertificateFactFromIndex \
    DirectMirBreakCfgCertificateFactInvalidExitSelection \
    DirectMirBreakCfgCertificateFactMutationRejected; do
    grep -Fq -- "$term" "$BREAK_FACT_OWNER" ||
        fail "break AIR fact lacks $term"
done
grep -Fq -- 'DirectMirBreakCfgShapeFactFromOwners' "$BREAK_SHAPE_OWNER" ||
    fail "break target-neutral shape projection is missing"
grep -Fq -- 'DirectMirBreakCfgPlanFactMutationRejected' "$BREAK_PLAN_OWNER" ||
    fail "break fixed-plan mutation ratchet is missing"
[[ "$(grep -R -F --include='*.pgy' \
    'DirectMirBreakCfgCertificateFactMutationRejected(' \
    "$ROOT_DIR/src/self_hosted" | wc -l | tr -d ' ')" == 2 ]] ||
    fail "break repaired-certificate negative must have one definition and one caller"
[[ "$(grep -R -F --include='*.pgy' \
    'DirectMirBreakCfgPlanFactMutationRejected(' \
    "$ROOT_DIR/src/self_hosted" | wc -l | tr -d ' ')" == 2 ]] ||
    fail "break repaired-plan negative must have one definition and one caller"
grep -Fq -- 'DirectMirBreakCfgEmitC' "$BREAK_EMISSION_OWNER" &&
    grep -Fq -- 'DirectMirBreakCfgEmitLlvm' "$BREAK_EMISSION_OWNER" ||
    fail "break emission responsibility must retain both backends"

require_file "$SOURCE"; produce_one_mir; mir_digest="$(hash_file "$MIR_ARTIFACT")"
[[ "$(wc -c <"$MIR_ARTIFACT" | tr -d ' ')" == 7054 && "$mir_digest" == \
    cb2d4f9fad6411ae9ce54e2d072d038735c29d2499a960909a09fae8eb59efbf && \
    "$(grep -o '"id":[0-9]*,"reachable":true' "$MIR_ARTIFACT" | wc -l | tr -d ' ')" == 6 && \
    "$(grep -o '"kind":"phi"' "$MIR_ARTIFACT" | wc -l | tr -d ' ')" == 1 ]] ||
    fail "break_after_stmt producer identity or six-block/one-phi shape drifted"
project_one_target c "$C_ARTIFACT" "$mir_digest"
project_one_target llvm "$LLVM_ARTIFACT" "$mir_digest"
for fact in 'long long pgy_local_0 = 0;' 'while (pgy_local_0 < 5)' \
    'pgy_local_0 = pgy_local_0 + 1;' 'if (pgy_local_0 == 3)' \
    'printf("%lld\n", (long long)pgy_local_0);' 'break;' 'return 0;'; do
    grep -Fq -- "$fact" "$C_ARTIFACT" || fail "break C fact drifted: $fact"
done
[[ "$(grep -o 'while (' "$C_ARTIFACT" | wc -l | tr -d ' ')" == 1 &&
    "$(grep -o 'if (' "$C_ARTIFACT" | wc -l | tr -d ' ')" == 1 &&
    "$(grep -F 'printf("%lld\n"' "$C_ARTIFACT" | wc -l | tr -d ' ')" == 2 &&
    "$(grep -F 'break;' "$C_ARTIFACT" | wc -l | tr -d ' ')" == 1 ]] ||
    fail "break C while/decision/Log topology drifted"
for fact in 'icmp slt i64' 'icmp eq i64' 'add i64' \
    'br label %pgy.block.1' 'label %pgy.block.3' 'label %pgy.block.4' \
    'label %pgy.block.5' \
    'backend-only exit value selection; not MIR evidence' \
    '%pgy.backend.exit.value = phi i64' 'ret i32 0'; do
    grep -Fq -- "$fact" "$LLVM_ARTIFACT" || fail "break LLVM fact drifted: $fact"
done
[[ "$(grep -F 'call i32 (ptr, ...) @printf' "$LLVM_ARTIFACT" | wc -l | tr -d ' ')" == 2 ]] ||
    fail "break LLVM must retain the inner and exit Log calls"
[[ "$(grep -F ' phi i64 ' "$LLVM_ARTIFACT" | wc -l | tr -d ' ')" == 2 ]] ||
    fail "break LLVM must contain one MIR phi and one backend-only exit phi"
grep -Fq -- '%pgy.backend.exit.value = phi i64 [ %pgy.local.loop, %pgy.block.1 ], [ %pgy.local.next, %pgy.block.3 ]' \
    "$LLVM_ARTIFACT" ||
    fail "break LLVM backend-only exit phi incoming lanes drifted"
compile_artifacts; run_and_compare $'3\n3'
assert_mir_identity "$mir_digest" "break_after_stmt backend executions"

# Execute both exit lanes from newly produced MIRs. A late break threshold
# reaches the normal header-false exit after iteration; an initial value at the
# limit reaches the same lane without entering the body. Dynamic local scope
# restores all artifact/source variables when each variant returns.
baseline_source="$SOURCE"; baseline_mir="$MIR_ARTIFACT"
baseline_c="$C_ARTIFACT"; baseline_llvm="$LLVM_ARTIFACT"
run_break_source_variant() {
    local variant_source="$1" variant_stem="$2" expected="$3" required_fact="$4"
    local SOURCE="$variant_source"
    local MIR_ARTIFACT="$WORK_DIR/$variant_stem.mir.json"
    local C_ARTIFACT="$WORK_DIR/$variant_stem.c"
    local LLVM_ARTIFACT="$WORK_DIR/$variant_stem.ll"
    local variant_digest
    require_file "$SOURCE"; produce_one_mir
    variant_digest="$(hash_file "$MIR_ARTIFACT")"
    [[ "$variant_digest" != "$mir_digest" &&
        "$(grep -o '"id":[0-9]*,"reachable":true' "$MIR_ARTIFACT" | wc -l | tr -d ' ')" == 6 &&
        "$(grep -o '"kind":"phi"' "$MIR_ARTIFACT" | wc -l | tr -d ' ')" == 1 ]] ||
        fail "$variant_stem did not produce a distinct six-block/one-phi MIR"
    grep -Fq -- "$required_fact" "$MIR_ARTIFACT" ||
        fail "$variant_stem MIR did not retain its source-owned literal"
    project_one_target c "$C_ARTIFACT" "$variant_digest"
    project_one_target llvm "$LLVM_ARTIFACT" "$variant_digest"
    compile_artifacts; run_and_compare "$expected"
    assert_mir_identity "$variant_digest" "$variant_stem backend executions"
}

late_source="$WORK_DIR/break_after_stmt.late.pgy"
sed 's/i == 3/i == 9/' "$baseline_source" >"$late_source"
grep -Fq -- 'if i == 9 {' "$late_source" ||
    fail "could not create late-break source"
run_break_source_variant "$late_source" break-after-stmt-late 5 '"text":"9"'

zero_trip_source="$WORK_DIR/break_after_stmt.zero-trip.pgy"
sed 's/let i: Int = 0;/let i: Int = 5;/' \
    "$baseline_source" >"$zero_trip_source"
grep -Fq -- 'let i: Int = 5;' "$zero_trip_source" ||
    fail "could not create zero-trip break source"
run_break_source_variant "$zero_trip_source" break-after-stmt-zero-trip 5 \
    '"expr0":"5","expr0_graph":{"root":0,"nodes":[{"kind":"integer_literal","text":"5"'

[[ "$SOURCE" == "$baseline_source" && "$MIR_ARTIFACT" == "$baseline_mir" &&
    "$C_ARTIFACT" == "$baseline_c" && "$LLVM_ARTIFACT" == "$baseline_llvm" ]] ||
    fail "break source variant did not restore baseline artifacts"

# Phi array order is not predecessor identity. The continuation predecessor b4
# forwards i.4 defined in b2; reversing storage order must not change output.
permuted="$(make_mutation break_phi_incoming_order \
    's/"uses":\["i\.1","i\.4"\]/"uses":["i.4","i.1"]/' \
    '"uses":["i.4","i.1"]')"
MIR_ARTIFACT="$permuted"
C_ARTIFACT="$WORK_DIR/break-after-stmt.permuted.c"
LLVM_ARTIFACT="$WORK_DIR/break-after-stmt.permuted.ll"
permuted_digest="$(hash_file "$MIR_ARTIFACT")"
project_one_target c "$C_ARTIFACT" "$permuted_digest"
project_one_target llvm "$LLVM_ARTIFACT" "$permuted_digest"
cmp -s "$baseline_c" "$C_ARTIFACT" && cmp -s "$baseline_llvm" "$LLVM_ARTIFACT" ||
    fail "break phi incoming order changed predecessor-forwarded artifacts"
MIR_ARTIFACT="$baseline_mir"; C_ARTIFACT="$baseline_c"; LLVM_ARTIFACT="$baseline_llvm"

mutation="$(make_mutation break_summary_flags \
    's/"flags":1/"flags":0/' '"flags":0')"
expect_rejected_without_artifact break_summary_flags "$mutation" \
    'break|loop|summary|CFG'
mutation="$(make_mutation break_block_identity \
    's/{"id":4,"reachable":true/{"id":6,"reachable":true/' \
    '{"id":6,"reachable":true')"
expect_rejected_without_artifact break_block_identity "$mutation" \
    'break|block|identity|order|CFG|program structure|machine[^[:alnum:]]+layer'
mutation="$(make_mutation break_nonempty_continuation \
    's/{"id":4,"reachable":true,"instructions":\[\]/{"id":4,"reachable":true,"instructions":[{"kind":"stmt"}]/' \
    '{"id":4,"reachable":true,"instructions":[{"kind":"stmt"}]')"
expect_rejected_without_artifact break_nonempty_continuation "$mutation" \
    'break|continuation|instruction|inventory|CFG|machine[^[:alnum:]]+layer'
mutation="$(make_mutation break_stale_phi_forward \
    's/"uses":\["i\.1","i\.4"\]/"uses":["i.1","i.5"]/' \
    '"uses":["i.1","i.5"]')"
expect_rejected_without_artifact break_stale_phi_forward "$mutation" \
    'break|phi|forward|incoming|SSA|definition'
mutation="$(make_mutation break_duplicate_phi_forward \
    's/"uses":\["i\.1","i\.4"\]/"uses":["i.1","i.1"]/' \
    '"uses":["i.1","i.1"]')"
expect_rejected_without_artifact break_duplicate_phi_forward "$mutation" \
    'break|phi|forward|incoming|predecessor'
mutation="$(make_mutation break_missing_continuation_backedge \
    's/{"id":4,"reachable":true,"instructions":\[\],"succ_true":1/{"id":4,"reachable":true,"instructions":[],"succ_true":5/' \
    '{"id":4,"reachable":true,"instructions":[],"succ_true":5')"
expect_rejected_without_artifact break_missing_continuation_backedge "$mutation" \
    'break|continuation|backedge|CFG|successor'
mutation="$(make_mutation break_redirected_break_edge \
    's/],"succ_true":5},{"id":4/],"succ_true":1},{"id":4/' \
    '],"succ_true":1},{"id":4')"
expect_rejected_without_artifact break_redirected_break_edge "$mutation" \
    'break|exit|edge|CFG|successor'
mutation="$(make_mutation break_split_exit_lane \
    's/,"succ_true":2,"succ_false":5/,"succ_true":2,"succ_false":4/' \
    '"succ_true":2,"succ_false":4')"
expect_rejected_without_artifact break_split_exit_lane "$mutation" \
    'break|normal|exit|lane|CFG|successor'
mutation="$(make_mutation break_outer_condition_use \
    's/"uses":\["i\.2"\]/"uses":["i.1"]/' '"uses":["i.1"]')"
expect_rejected_without_artifact break_outer_condition_use "$mutation" \
    'break|loop|condition|phi|use|SSA'
mutation="$(make_mutation break_increment_use \
    's/"uses":\["i\.2"\]/"uses":["i.1"]/2' '"uses":["i.1"]')"
expect_rejected_without_artifact break_increment_use "$mutation" \
    'break|increment|assignment|use|SSA'
mutation="$(make_mutation break_increment_target_graph \
    's/"expr1_graph":{"root":0,"nodes":\[{"kind":"leaf","text":"i"/"expr1_graph":{"root":0,"nodes":[{"kind":"leaf","text":"j"/' \
    '"expr1_graph":{"root":0,"nodes":[{"kind":"leaf","text":"j"')"
expect_rejected_without_artifact break_increment_target_graph "$mutation" \
    'break|increment|assignment|target|graph|local'
mutation="$(make_mutation break_decision_use \
    's/"uses":\["i\.4"\]/"uses":["i.2"]/' '"uses":["i.2"]')"
expect_rejected_without_artifact break_decision_use "$mutation" \
    'break|decision|condition|use|SSA'
mutation="$(make_mutation break_inner_log_use \
    's/"uses":\["i\.4"\]/"uses":["i.2"]/2' '"uses":["i.2"]')"
expect_rejected_without_artifact break_inner_log_use "$mutation" \
    'break|inner|Log|use|SSA'
mutation="$(make_mutation break_exit_log_use \
    's/"uses":\["i\.4"\]/"uses":["i.2"]/3' '"uses":["i.2"]')"
expect_rejected_without_artifact break_exit_log_use "$mutation" \
    'break|exit|Log|selection|lane|use|SSA'
mutation="$(make_mutation break_row_kind \
    's/"kind":"branch","name":"break"/"kind":"stmt","name":"break"/' \
    '"kind":"stmt","name":"break"')"
expect_rejected_without_artifact break_row_kind "$mutation" \
    'break|branch|instruction|identity|CFG'
mutation="$(make_mutation break_row_source \
    's/"source_type":"AST_BREAK"/"source_type":"AST_CONTINUE"/' \
    '"source_type":"AST_CONTINUE"')"
expect_rejected_without_artifact break_row_source "$mutation" \
    'break|source|identity|AST_BREAK|CFG'
mutation="$(make_mutation break_row_name \
    's/"kind":"branch","name":"break"/"kind":"branch","name":"continue"/' \
    '"kind":"branch","name":"continue"')"
expect_rejected_without_artifact break_row_name "$mutation" \
    'break|name|identity|instruction|CFG'
mutation="$(make_mutation break_outer_operator \
    's/"kind":"less"/"kind":"greater"/' '"kind":"greater"')"
expect_rejected_without_artifact break_outer_operator "$mutation" \
    'break|loop|condition|operator|graph'
mutation="$(make_mutation break_increment_operator \
    's/"kind":"add"/"kind":"multiply"/' '"kind":"multiply"')"
expect_rejected_without_artifact break_increment_operator "$mutation" \
    'break|increment|operator|graph'
mutation="$(make_mutation break_decision_operator \
    's/"kind":"equality"/"kind":"less"/' '"kind":"less"')"
expect_rejected_without_artifact break_decision_operator "$mutation" \
    'break|decision|condition|operator|graph'
mutation="$(make_mutation break_inner_log_target \
    's/"call_target_name":"ToString"/"call_target_name":"ToInt"/' \
    '"call_target_name":"ToInt"')"
expect_rejected_without_artifact break_inner_log_target "$mutation" \
    'break|inner|Log|ToString|target|graph'
mutation="$(make_mutation break_exit_log_target \
    's/"call_target_name":"ToString"/"call_target_name":"ToInt"/2' \
    '"call_target_name":"ToInt"')"
expect_rejected_without_artifact break_exit_log_target "$mutation" \
    'break|exit|Log|ToString|target|graph'

echo "[$LABEL] ifelse + if_else_assign + reassign_block + nestedif + whileloop + forloop + break_after_stmt one-MIR CFG/AIR-plan gate ok (sha256=$mir_digest)"

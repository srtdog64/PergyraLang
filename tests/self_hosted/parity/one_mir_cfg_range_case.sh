# Compatibility hook: range execution is owned by the general scalar CFG plan.

SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/forloop.pgy"
RANGE_AIR_OWNER="$ROOT_DIR/src/self_hosted/air/mir_range_cfg_certificate_fact_owner.pgy"
RANGE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_range_iteration_owner.pgy"
RANGE_EMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_range_emission_owner.pgy"
GENERAL_ROUTE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_route_owner.pgy"
for owner in "$RANGE_AIR_OWNER" "$RANGE_OWNER" "$RANGE_EMISSION" \
    "$GENERAL_ROUTE"; do
    require_file "$owner"
done
grep -Fq -- 'DirectMirRangeCfgCertificateFactMutationRejected' "$RANGE_AIR_OWNER" ||
    fail "range AIR evidence mutation ratchet is missing"
grep -Fq -- 'DirectMirScalarCfgRangeIterationFactsFromOwners' "$RANGE_OWNER" ||
    fail "general scalar CFG range receipt is missing"
grep -Fq -- 'DirectMirScalarCfgRangeCBlockEffect' "$RANGE_EMISSION" &&
    grep -Fq -- 'DirectMirScalarCfgRangeLlvmBlockEffect' "$RANGE_EMISSION" ||
    fail "general range block effect must retain both backends"
for retired in direct_mir_range_cfg_shape_owner.pgy \
    direct_mir_range_cfg_plan_fact_owner.pgy \
    direct_mir_range_cfg_emission_owner.pgy; do
    [[ ! -e "$ROOT_DIR/src/self_hosted/compiler/$retired" ]] ||
        fail "retired range mini-compiler reappeared: $retired"
done

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
[[ "$(grep -o '"id":[0-9]*,"reachable":true' "$MIR_ARTIFACT" | wc -l | tr -d ' ')" == 4 ]] ||
    fail "forloop producer did not retain its four reachable blocks"
project_one_target c "$C_ARTIFACT" "$mir_digest"
project_one_target llvm "$LLVM_ARTIFACT" "$mir_digest"
grep -Fq -- 'goto pgy_block_1;' "$C_ARTIFACT" ||
    fail "range C did not use the general graph emitter"
grep -Fq -- '%pgy.range.' "$LLVM_ARTIFACT" ||
    fail "range LLVM did not consume the general range receipt"
compile_artifacts; run_and_compare $'0\n1\n2'
assert_mir_identity "$mir_digest" "forloop general graph backend executions"

baseline_mir="$MIR_ARTIFACT"; baseline_c="$C_ARTIFACT"; baseline_llvm="$LLVM_ARTIFACT"
generalized="$(make_mutation generalized_range \
    's/"expr0":"0"/"expr0":"2"/g;s/"text":"0"/"text":"2"/g;s/"expr1":"3"/"expr1":"5"/g;s/"text":"3"/"text":"5"/g' \
    '"text":"5"')"
MIR_ARTIFACT="$generalized"; C_ARTIFACT="$WORK_DIR/forloop.generalized.c"
LLVM_ARTIFACT="$WORK_DIR/forloop.generalized.ll"
generalized_digest="$(hash_file "$MIR_ARTIFACT")"
project_one_target c "$C_ARTIFACT" "$generalized_digest"
project_one_target llvm "$LLVM_ARTIFACT" "$generalized_digest"
compile_artifacts; run_range_projected_and_compare $'2\n3\n4'
assert_mir_identity "$generalized_digest" "generalized range executions"
MIR_ARTIFACT="$baseline_mir"; C_ARTIFACT="$baseline_c"; LLVM_ARTIFACT="$baseline_llvm"

zero_trip="$(make_mutation zero_trip_range \
    's/"expr0":"0"/"expr0":"3"/g;s/"text":"0"/"text":"3"/g' \
    '"expr0":"3"')"
MIR_ARTIFACT="$zero_trip"; C_ARTIFACT="$WORK_DIR/forloop.zero-trip.c"
LLVM_ARTIFACT="$WORK_DIR/forloop.zero-trip.ll"
zero_trip_digest="$(hash_file "$MIR_ARTIFACT")"
project_one_target c "$C_ARTIFACT" "$zero_trip_digest"
project_one_target llvm "$LLVM_ARTIFACT" "$zero_trip_digest"
compile_artifacts; run_range_projected_and_compare ''
assert_mir_identity "$zero_trip_digest" "zero-trip range executions"
MIR_ARTIFACT="$baseline_mir"; C_ARTIFACT="$baseline_c"; LLVM_ARTIFACT="$baseline_llvm"

mutation="$(make_mutation range_summary_kind \
    's/"kind":"for"/"kind":"while"/' '"kind":"while"')"
expect_rejected_without_artifact range_summary_kind "$mutation" 'range|loop|summary|CFG'
mutation="$(make_mutation range_iteration_count \
    's/"iteration_type_fact_count":1/"iteration_type_fact_count":0/' \
    '"iteration_type_fact_count":0')"
expect_rejected_without_artifact range_iteration_count "$mutation" 'range|iteration|count|CFG'
mutation="$(make_mutation range_iteration_type \
    's/"binding_type":"Int"/"binding_type":"String"/' \
    '"binding_type":"String"')"
expect_rejected_without_artifact range_iteration_type "$mutation" 'range|iteration|type|Int'
mutation="$(make_mutation range_iteration_hoist \
    's/"collection_hoisted":false/"collection_hoisted":true/' \
    '"collection_hoisted":true')"
expect_rejected_without_artifact range_iteration_hoist "$mutation" 'range|iteration|hoist'
mutation="$(make_mutation range_missing_backedge \
    's/],"succ_true":1},{"id":3/],"succ_true":3},{"id":3/' \
    '],"succ_true":3},{"id":3')"
expect_rejected_without_artifact range_missing_backedge "$mutation" 'range|backedge|CFG'
mutation="$(make_mutation range_start_graph \
    's/"text":"0"/"text":"2"/' '"text":"2"')"
expect_rejected_without_artifact range_start_graph "$mutation" 'range|start|graph|literal'
mutation="$(make_mutation range_stop_graph \
    's/"text":"3"/"text":"4"/' '"text":"4"')"
expect_rejected_without_artifact range_stop_graph "$mutation" 'range|stop|graph|literal'
mutation="$(make_mutation range_binding \
    's/"arg0":"i"/"arg0":"j"/2' '"arg0":"j"')"
expect_rejected_without_artifact range_binding "$mutation" 'range|local|binding|branch'

echo "[$LABEL] forloop is owned by the general scalar CFG range receipt (sha256=$mir_digest)"
source "$ROOT_DIR/tests/self_hosted/parity/one_mir_cfg_break_case.sh"

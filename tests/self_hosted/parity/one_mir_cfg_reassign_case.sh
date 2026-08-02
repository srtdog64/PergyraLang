# Sourced by one_mir_cfg_air_plan_projection.sh after the v69 cases.
# Keeps the executable reassign rung distinct from the shared gate machinery.

SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/reassign_block.pgy"
require_file "$SOURCE"; produce_one_mir; mir_digest="$(hash_file "$MIR_ARTIFACT")"
[[ "$(wc -c <"$MIR_ARTIFACT" | tr -d ' ')" == 4090 && "$mir_digest" == \
    fba038d0ac0e502a7336b66abf184d6a400756f78069875d16d2c18f91ae746f && \
    "$(grep -o '"kind":"phi"' "$MIR_ARTIFACT" | wc -l | tr -d ' ')" == 1 ]] ||
    fail "reassign_block producer identity or [2,1,2]/one-phi shape drifted"
project_one_target c "$C_ARTIFACT" "$mir_digest"
project_one_target llvm "$LLVM_ARTIFACT" "$mir_digest"
compile_artifacts; run_and_compare 10
assert_mir_identity "$mir_digest" "reassign_block backend executions"

# Pin the three-block distinction: the entry false edge carries x.1 while the
# true predecessor carries x.3. Certificate/plan self-mutations also execute
# on each successful projection before either emitter opens its artifact.
mutation="$(make_mutation missing_false_edge \
    's/,"succ_false":2/,"succ_false_removed":2/' '"succ_false_removed":2')"
expect_rejected_without_artifact missing_false_edge "$mutation" \
    'CFG|successor|conditional[^[:alnum:]]+edge'
mutation="$(make_mutation wrong_true_predecessor \
    's/],"succ_true":2}/],"succ_true":1}/' '],"succ_true":1}')"
expect_rejected_without_artifact wrong_true_predecessor "$mutation" \
    'CFG|predecessor|phi|merge|successor'
mutation="$(make_mutation missing_false_predecessor_phi \
    's/"uses":\["x\.3","x\.1"\]/"uses":["x.3","x.3"]/' \
    '"uses":["x.3","x.3"]')"
expect_rejected_without_artifact missing_false_predecessor_phi "$mutation" \
    'phi.*(incoming|predecessor)|incoming.*predecessor'
mutation="$(make_mutation assignment_abi \
    's/"arg1":"local","slot_anchor":null,"abi_type_name":"Int"/"arg1":"local","slot_anchor":null,"abi_type_name":"String"/g' \
    '"arg1":"local","slot_anchor":null,"abi_type_name":"String"')"
expect_rejected_without_artifact assignment_abi "$mutation" \
    'CFG.*(arm|type)|arm.*(ABI|type)|ABI.*assignment'

source "$ROOT_DIR/tests/self_hosted/parity/one_mir_cfg_nested_case.sh"

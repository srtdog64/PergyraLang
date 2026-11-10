# The positive corpus and its falsifiers consume one admitted MIR artifact per
# case from one_mir_dual_backend_projection.sh. This file is sourced after the
# driver, backend, and ownership checks; it is not an independent entrypoint.

select_case hello "$ROOT_DIR/examples/hello.pgy"
run_positive_case 'Hello, Pergyra!'
grep -Fq '"expr0_graph":{' "$MIR_ARTIFACT" || fail "hello graph missing"
grep -Fq '"kind":"stmt"' "$MIR_ARTIFACT" || fail "hello kind missing"
mutation="$(make_mutation expr0_graph \
    's/"expr0_graph"/"expr0_graph_removed"/g' '"expr0_graph_removed"')"
expect_rejected_without_artifact expr0_graph "$mutation" \
    'expr0_graph|expression graph|direct MIR scalar CFG program expression admission is invalid: stage=graph-sequence'
mutation="$(make_mutation instruction_kind \
    's/"kind":"stmt"/"kind":"invalid-one-mir-gate"/g' \
    '"kind":"invalid-one-mir-gate"')"
expect_rejected_without_artifact instruction_kind "$mutation" \
    'instruction[^[:alnum:]]+(kind|identity)|kind[^[:alnum:]]+instruction|invalid instruction'
expect_rejected_without_artifact invalid_target "$MIR_ARTIFACT" \
    'target|backend' "--mir-json-backend=invalid"

select_case let_log "$ROOT_DIR/src/self_hosted/mir_lower/fixture/let_log.pgy"
run_positive_case '42'
mutation="$(make_mutation local_result_identity \
    's/"result":"x\.1"/"result":"x.2"/' '"result":"x.2"')"
expect_rejected_without_artifact local_result_identity "$mutation" \
    'local result identity|result identity|result[^[:alnum:]]+x\.1|direct MIR scalar CFG program expression admission is invalid: stage=leaf-operand'
mutation="$(make_mutation graph_use_edge \
    's/"uses":\["x\.1"\]/"uses":["x.2"]/' '"uses":["x.2"]')"
expect_rejected_without_artifact graph_use_edge "$mutation" \
    'graph use edge|use edge|uses[^[:alnum:]]+x\.1|direct MIR scalar CFG program expression admission is invalid: stage=leaf-operand'
mutation="$(make_mutation missing_use_fact \
    's/"uses":\["x\.1"\]/"uses_removed":["x.1"]/' \
    '"uses_removed":["x.1"]')"
expect_rejected_without_artifact missing_use_fact "$mutation" \
    'use facts|graph use edge|uses|direct MIR scalar CFG typed index, use, or phi facts are invalid'
mutation="$(make_mutation arithmetic_add_node \
    's/"kind":"add"/"kind":"subtract"/' \
    '"kind":"subtract"')"
# Changing an admitted arithmetic node is a semantic mutation, not malformed
# MIR. Both target artifacts must change and execute the same new result.
assert_semantic_operator_mutation "$mutation" arithmetic-subtract 0
mutation="$(make_mutation tostring_call_target \
    's/"call_target_name":"ToString"/"call_target_name":"NoSuchTarget"/' \
    '"call_target_name":"NoSuchTarget"')"
expect_rejected_without_artifact tostring_call_target "$mutation" \
    'ToString call target|call target[^[:alnum:]]+ToString|direct MIR scalar CFG program expression admission is invalid: stage=leaf-operand'

select_case multilet "$ROOT_DIR/src/self_hosted/mir_lower/fixture/multilet.pgy"
run_positive_case $'35\n12'
mutation="$(make_mutation second_local_result_use \
    's/"result":"b\.1"/"result":"c.1"/' '"result":"c.1"')"
expect_rejected_without_artifact second_local_result_use "$mutation" \
    'second local|local result|result.*use|b\.1|direct MIR scalar CFG result definition LocalRef is invalid'
mutation="$(make_mutation multiply_operator \
    's/"kind":"multiply"/"kind":"divide"/' '"kind":"divide"')"
assert_semantic_operator_mutation "$mutation" multiply-divide $'0\n12'
mutation="$(make_mutation statement_order \
    's/"id":2,"kind":"stmt"/"id":99,"kind":"stmt"/;s/"id":3,"kind":"stmt"/"id":2,"kind":"stmt"/;s/"id":99,"kind":"stmt"/"id":3,"kind":"stmt"/' \
    '"id":3,"kind":"stmt"')"
# Exchanging two valid, unreferenced statement IDs is an admitted identity
# relabel, not an execution-order mutation. The artifacts must stay identical.
original_mir="$MIR_ARTIFACT"
MIR_ARTIFACT="$mutation"
run_projection c "$WORK_DIR/$CASE.id-relabel.c"
run_projection llvm "$WORK_DIR/$CASE.id-relabel.ll"
cmp -s "$C_ARTIFACT" "$WORK_DIR/$CASE.id-relabel.c" ||
    fail "$CASE C artifact changed under unreferenced ID relabel"
cmp -s "$LLVM_ARTIFACT" "$WORK_DIR/$CASE.id-relabel.ll" ||
    fail "$CASE LLVM artifact changed under unreferenced ID relabel"
MIR_ARTIFACT="$original_mir"

select_case owner_handle_array_push \
    "$ROOT_DIR/tests/self_hosted/fixtures/direct_mir_owner_handle_array_push.pgy"
run_positive_case '1'
grep -Fq '"carriage":"owner-handle"' "$MIR_ARTIFACT" ||
    fail "owner-handle array push lost its formal carriage"
grep -Fq '"collection_mutation_target_graph":{' "$MIR_ARTIFACT" ||
    fail "owner-handle array push lost its target identity"
mutation="$(make_mutation owner_handle_carriage \
    's/"carriage":"owner-handle"/"carriage":"value"/' \
    '"carriage":"value"')"
expect_rejected_without_artifact owner_handle_carriage "$mutation" \
    'MIR-JSON|routine signature|owner-handle|collection mutation|stage=statement'
mutation="$(make_mutation owner_handle_pass \
    's/"pass":"direct"/"pass":"indirect"/' \
    '"pass":"indirect"')"
expect_rejected_without_artifact owner_handle_pass "$mutation" \
    'MIR-JSON|pass shape|owner-handle|collection mutation|stage=statement'
mutation="$(make_mutation owner_handle_abi \
    's/"abi_layout":{"type":"Array<String>","size":32,"align":8/"abi_layout":{"type":"Array<String>","size":32,"align":16/' \
    '"type":"Array<String>","size":32,"align":16')"
expect_rejected_without_artifact owner_handle_abi "$mutation" \
    'MIR-JSON|ABI|layout|owner-handle|stage=statement|routine-instance|inventory signature|scalar program route'
mutation="$(make_mutation owner_handle_receiver \
    's/"binding_kind":"formal_parameter","binding_ordinal":0/"binding_kind":"formal_parameter","binding_ordinal":1/' \
    '"binding_kind":"formal_parameter","binding_ordinal":1')"
expect_rejected_without_artifact owner_handle_receiver "$mutation" \
    'MIR-JSON|receiver|parameter|collection mutation|stage=statement'

select_case owned_string_clone_array \
    "$ROOT_DIR/tests/self_hosted/fixtures/direct_mir_owned_string_clone_array.pgy"
run_positive_case '2'
grep -Fq '"element_ownership":"owned-elements","disposition":"live","origin":"owned-literal"' \
    "$MIR_ARTIFACT" || fail "owned String clone array lost its ownership fact"
grep -Fq '"local_ref":"declaration:6:0"' "$MIR_ARTIFACT" ||
    fail "owned String clone array lost its LocalRef identity"
mutation="$(make_mutation owned_string_clone_origin \
    '0,/"element_ownership":"owned-elements","disposition":"live","origin":"owned-literal"/s//"element_ownership":"unknown","disposition":"live","origin":"unknown"/' \
    '"element_ownership":"unknown","disposition":"live","origin":"unknown"')"
expect_rejected_without_artifact owned_string_clone_origin "$mutation" \
    'ownership|LocalRef|replacement|owned-literal|scalar program route'
mutation="$(make_mutation owned_string_clone_call_identity \
    '0,/"call_target_name":"Clone"/s//"call_target_name":"NoSuchClone"/' \
    '"call_target_name":"NoSuchClone"')"
expect_rejected_without_artifact owned_string_clone_call_identity "$mutation" \
    'Clone|call target|builtin-call|expression admission|scalar program route'

echo "[$LABEL] hello + let_log + multilet + owner-handle array push + owned String clone array one-MIR dual-backend gate ok"

# Platform-full harness evidence. The installed toolchain is supplied by the
# shard; each script runs in its own process so one red cannot hide the rest.

run 'bash tests/arena_ledger_smoke.sh'
run 'bash tests/self_hosted/parity/world_zone_admission_owner.sh'
run 'bash tests/self_hosted/parity/hashmap_runtime_owner.sh'
run 'bash tests/self_hosted/parity/callable_capability_fact_owner.sh'
run 'bash tests/self_hosted/parity/collection_call_protocol_owner_smoke.sh'
run 'bash tests/self_hosted/parity/domain_topology_graph_plan_consumer_owner.sh'
run 'bash tests/self_hosted/parity/driver_rung2_canonical_identity_epoch_owner.sh'
run 'bash tests/self_hosted/parity/intent_capability_fact_owner.sh'
run 'bash tests/self_hosted/parity/mir_receiver_carriage_admission_owner.sh'
run 'bash tests/self_hosted/parity/one_mir_string_array_index_return_projection.sh'
run 'bash tests/self_hosted/parity/one_mir_string_case_math_projection.sh'
run 'bash tests/self_hosted/parity/one_mir_string_trim_projection.sh'
run 'bash tests/self_hosted/parity/one_mir_string_window_builtin_projection.sh'
run 'bash tests/self_hosted/parity/parser_language_word_registry_parity.sh'
run 'bash tests/self_hosted/parity/semantic_expression_normalization_owner_smoke.sh'
run 'bash tests/self_hosted/parity/semantic_expression_validation_lifetime_owner_smoke.sh'

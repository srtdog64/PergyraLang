#ifdef PGY_LLVM_ENABLED
    printf("\n[LLVM backend]\n");
    run_pipeline_case("projection_abi", projection_source, projection_expected,
                      "0 error(s), 2 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("zone_projection_abi", zone_projection_source, zone_projection_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("intent_trace_abi", intent_source, intent_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("intent_recent_abi", intent_recent_source, intent_recent_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("intent_active_abi", intent_active_source, intent_active_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("intent_failure_abi", intent_failure_source, intent_failure_expected,
                      "0 error(s), 1 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("world_clone_ownership_abi", world_clone_source, world_clone_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("world_handoff_mutation_abi", world_handoff_mutation_source, world_handoff_mutation_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("intent_authority_snapshot_abi", intent_authority_snapshot_source, intent_authority_snapshot_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("handoff_projection_frontier_abi", handoff_projection_frontier_source, handoff_projection_frontier_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("handoff_world_state_frontier_abi", handoff_world_state_frontier_source, handoff_world_state_frontier_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("handoff_layer_state_frontier_abi", handoff_layer_state_frontier_source, handoff_layer_state_frontier_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("world_zone_query_abi", world_zone_query_source, world_zone_query_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("world_fixpoint_abi", world_fixpoint_source, world_fixpoint_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("projection_chain_abi", projection_chain_source, projection_chain_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("zone_frontier_abi", zone_frontier_source, zone_frontier_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("world_embedded_projection_abi", world_embedded_projection_source, world_embedded_projection_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("world_embedded_method_projection_abi", world_embedded_method_projection_source, world_embedded_method_projection_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("world_embedded_branch_projection_abi", world_embedded_branch_projection_source, world_embedded_branch_projection_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("world_embedded_action_frontier_abi", world_embedded_action_frontier_source, world_embedded_action_frontier_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("world_embedded_action_pool_frontier_abi", world_embedded_action_pool_frontier_source, world_embedded_action_pool_frontier_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("authority_failure_abi", authority_failure_source, authority_failure_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("relation_effect_zone_abi", relation_effect_zone_source, relation_effect_zone_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("relation_effect_propagation_abi", relation_effect_propagation_source, relation_effect_propagation_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_same_process_repeat_case("relation_effect_propagation_reentry_abi",
                                 relation_effect_propagation_source,
                                 relation_effect_propagation_expected,
                                 "0 error(s), 0 warning(s)",
                                 BACKEND_LLVM, !perf_mode, 45.0, 5.0, 3);
    run_pipeline_case("runtime_floor", loop_source, loop_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    if (perf_mode) {
        run_pipeline_case("projection_medium", projection_medium_source, projection_medium_expected,
                          "0 error(s), 0 warning(s)",
                          BACKEND_LLVM, false, 90.0, 10.0);
        run_pipeline_case("intent_medium", intent_medium_source, intent_medium_expected,
                          "0 error(s), 0 warning(s)",
                          BACKEND_LLVM, false, 90.0, 10.0);
        run_pipeline_case("rollback_medium", rollback_medium_source, rollback_medium_expected,
                          "0 error(s), 0 warning(s)",
                          BACKEND_LLVM, false, 90.0, 10.0);
        run_pipeline_case("transfer_medium", transfer_medium_source, transfer_medium_expected,
                          "0 error(s), 0 warning(s)",
                          BACKEND_LLVM, false, 90.0, 10.0);
    }
#endif

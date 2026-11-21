# Fast Linux push gate. Full platform coverage remains in ci_linux_steps.sh.

run 'make check-build-tools CC="$CI_LINUX_CC"'
run 'make check-linux-toolchain'
case "${PGY_CI_SELF_HOST_MODE:-build}" in
    build)
        run 'make CC="$CI_LINUX_CC" self-host-compiler'
        ;;
    prebuilt)
        if [[ ! -x "$PWD/bin/pgy" || ! -x "$PWD/bin/pgy-self-driver" ||
              ! -s "$PWD/bin/pgy-self-driver.machine-layer-manifest.json" ]]; then
            echo "ci-push-linux: prebuilt self-host toolchain artifact is incomplete" >&2
            exit 1
        fi
        # Every make target below that depends on self-host-compiler admits
        # this pair instead of rebuilding DRV-2 (Makefile, self-host-compiler).
        export PGY_SELF_HOST_COMPILER_ADMITTED=1
        ;;
    *)
        echo "ci-push-linux: invalid PGY_CI_SELF_HOST_MODE=${PGY_CI_SELF_HOST_MODE}" >&2
        exit 2
        ;;
esac
export PGY_SELF_DRIVER_BIN="$PWD/bin/pgy-self-driver"

PGY_CI_PUSH_LINUX_SHARD="${PGY_CI_PUSH_LINUX_SHARD:-all}"
PGY_CI_PUSH_LINUX_RUN_CORE=0
PGY_CI_PUSH_LINUX_RUN_SELF_HOST=0
case "$PGY_CI_PUSH_LINUX_SHARD" in
    all)
        PGY_CI_PUSH_LINUX_RUN_CORE=1
        PGY_CI_PUSH_LINUX_RUN_SELF_HOST=1
        ;;
    core)
        PGY_CI_PUSH_LINUX_RUN_CORE=1
        ;;
    self-host)
        PGY_CI_PUSH_LINUX_RUN_SELF_HOST=1
        ;;
    *)
        echo "ci-push-linux: invalid PGY_CI_PUSH_LINUX_SHARD=$PGY_CI_PUSH_LINUX_SHARD; expected all, core, or self-host" >&2
        exit 2
        ;;
esac

if [[ "$PGY_CI_PUSH_LINUX_RUN_CORE" == "1" ]]; then
    run 'make self-host-llvm-option-member-assignment-context-test-smoke'
    run 'make self-host-llvm-intent-value-argument-abi-test-smoke'
    run 'make self-host-replacement-frontier-installed-test-smoke'
    run 'bash tests/self_hosted/parity/direct_mir/direct_mir_array_named_value_boundary_owner.sh'
    run 'bash tests/self_hosted/parity/ownership/string_interpolation_escape_parity_owner.sh'
    run 'bash tests/self_hosted/parity/semantic/match_subject_family_owner.sh'
    run 'bash tests/self_hosted/parity/semantic/match_or_pattern_owner.sh'
    run 'bash tests/self_hosted/parity/ownership/never_return_type_owner.sh'
    run 'bash tests/self_hosted/parity/frontend/missing_return_flow_owner.sh'
    run 'bash tests/self_hosted/parity/semantic/enum_variant_identity_owner.sh'
    run 'bash tests/self_hosted/parity/ownership/destructure_temporary_hygiene_owner.sh'
    run 'bash tests/self_hosted/parity/ownership/defer_multi_statement_owner.sh'
    run 'bash tests/self_hosted/parity/semantic/builtin_name_shadow_owner.sh'
    run 'bash tests/self_hosted/parity/ownership/text_conversion_argument_owner.sh'
    run 'bash tests/self_hosted/parity/semantic/default_route_scalar_value_owner.sh'
    run 'bash tests/self_hosted/parity/codegen/runtime_panic_leg_owner.sh'
    run 'bash tests/self_hosted/parity/codegen/entrypoint_exit_status_leg_owner.sh'
    run 'bash tests/self_hosted/parity/direct_mir/direct_mir_general_emitter_coverage_owner.sh'
    run 'bash tests/self_hosted/parity/direct_mir/direct_mir_general_emitter_builtin_rows_owner.sh'
    run 'bash tests/self_hosted/parity/one_mir/one_mir_array_param_projection.sh'
    run 'bash tests/self_hosted/parity/direct_mir/direct_mir_runtime_helper_materialization_owner.sh'
    run 'bash tests/self_hosted/parity/direct_mir/direct_mir_general_route_collection_iteration_owner.sh'
    run 'bash tests/self_hosted/parity/ownership/element_release_loan_refusal_owner.sh'
    run 'bash tests/self_hosted/parity/direct_mir/direct_mir_general_route_collection_programs_owner.sh'
    run 'bash tests/self_hosted/parity/direct_mir/direct_mir_mutable_declaration_owner.sh'
    run 'bash tests/self_hosted/parity/direct_mir/default_route_native_runtime_parity_owner.sh'
    run 'bash tests/self_hosted/parity/mir/phi_dominance_reuse_owner.sh'
    run 'bash tests/self_hosted/parity/mir/json_projection_output_lifetime.sh'
    run 'bash tests/self_hosted/parity/mir/json_projection_join_contract.sh'
    run 'bash tests/self_hosted/parity/codegen/array_growth_allocation_failure.sh'
    run 'bash tests/self_hosted/parity/driver/selfhost_compile_memory_owner.sh'
    run 'bash tests/self_hosted/parity/one_mir/one_mir_string_case_math_projection.sh'
    run 'bash tests/self_hosted/parity/semantic/default_route_method_receiver_order_owner.sh'
    run 'bash tests/self_hosted/parity/driver/parent_relative_output_path_owner.sh'
    run 'bash tests/self_hosted/parity/codegen/extern_block_identity_owner.sh'
    run 'bash tests/self_hosted/parity/driver/single_owner_handle_owner.sh'
    run 'bash tests/self_hosted/parity/codegen/containment_embedding_copy_owner.sh'
    run 'make text-builder-owner-test-smoke'
    run 'bash tests/self_hosted/parity/codegen/numeric_literal_conversion_owner.sh'
    run 'bash tests/self_hosted/parity/ownership/loop_stack_storage_owner.sh'
    run 'bash tests/self_hosted/parity/semantic/default_llvm_unwrap_panic_owner.sh'
    run 'bash tests/backend/llvm_runtime_entrypoint_attrs_smoke.sh'
    run 'bash tests/self_hosted/parity/semantic/default_route_empty_literal_site_owner.sh'
    run 'bash tests/self_hosted/parity/semantic/default_route_inout_prototype_payload_owner.sh'
    run 'bash tests/self_hosted/parity/frontend/builtin_surface_parity_owner.sh'
    run 'bash tests/self_hosted/parity/driver/io_result_builtin_owner.sh'
    run 'bash tests/self_hosted/parity/semantic/default_route_diagnostic_position_owner.sh'
    run 'bash tests/self_hosted/parity/semantic/reserved_name_ref_alias_refusal_owner.sh'

    run 'make build-source-inventory-test-smoke'
    run 'make gate-subject-declaration-test-smoke'
    run 'make gate-script-reachability-test-smoke'
    run 'make builtin-effect-registry-test-smoke'
    run 'make language-keyword-registry-test-smoke'
    run 'make call-handoff-rule-test-smoke'
    run 'make native-unsafe-negative-refusal-test-smoke'
    run 'make foreach-string-borrow-refusal-test-smoke'
    run 'make inout-call-string-loan-refusal-test-smoke'
    run 'make unknown-string-store-copy-test-smoke'
    run 'make readonly-map-query-test-smoke'
    run 'make long-literal-exactness-test-smoke'
    run 'make string-window-extent-test-smoke'
    run 'make call-argument-evaluation-order-test-smoke'
    run 'make self-host-driver-fixed-point-receipt-test-smoke'
    run 'make self-host-codegen-seed-receipt-test-smoke'
    run 'make self-host-domain-runtime-zone-sync-test-smoke'
    run 'make ci-step-runner-test-smoke'
    run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" release-primary-debug-section-hygiene-test-smoke'
    run 'make grammar-examples-compile-test-smoke'
    run 'make source-utf8-test-smoke'
    run 'make backend-fail-closed-test-smoke'
    run 'make worker-boundary-ub-test-smoke'
    run 'make parallel-capture-reach-test-smoke'
    run 'make generic-falsification-test-smoke'
    run 'make CC="$CI_LINUX_CC" runtime-spawn-context-propagation-test-smoke'
    run 'make CC="$CI_LINUX_CC" structured-spawn-lifecycle-test-smoke'
    run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" clean'
    run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" test-all'
    run 'make CC="$CI_LINUX_CC" BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" hir-function-param-flow-summary-test-smoke air-function-param-flow-summary-test-smoke'
fi

if [[ "$PGY_CI_PUSH_LINUX_RUN_SELF_HOST" == "1" ]]; then
    run 'make self-host-identity-cell-receiver-test-smoke'
    run 'make self-host-collection-ownership-semantic-test-smoke'
    run 'make self-host-direct-mir-array-string-cleanup-test-smoke'
    run 'make self-host-shared-storage-copy-landing-test-smoke'
    run 'make self-host-mir-round-trip-semantic-test-smoke'
    run 'make self-host-task-is-cancelled-builtin-test-smoke'
    run 'make self-host-slice-copy-semantic-bridge-test-smoke'
    run 'make self-host-zone-spawn-transport-admission-test-smoke'
    run 'make self-host-future-aggregate-storage-admission-test-smoke'
    run 'make effect-admission-test-smoke'
    run 'make self-host-preparation-contract-test-smoke'
    run 'make self-host-mir-function-param-flow-summary-test-smoke'
fi

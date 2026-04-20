/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Diagnostic code / cause_ir / fix_source registry.
 *
 * Single source of truth for stable diagnostic literals.  Adding a new
 * literal at a call site WITHOUT first defining it here is a code-review
 * smell — downstream consumers need a stable set to route on, and
 * docs/72_diagnostic_codes.md mirrors this header.
 *
 * Usage:
 *
 *     #include "diag_codes.h"
 *     ...
 *     semantic_error_with_hints(ctx,
 *         PGY_CODE_SEM_TYPE_MISMATCH,
 *         PGY_CAUSE_CONDITION_NON_BOOL,
 *         PGY_FIX_CONVERT_TO_BOOL,
 *         node, "if condition must be Bool, got '%s'", t->name);
 *
 * Migration of existing string-literal call sites is incremental.
 * New diagnostic sites MUST use the macros from this header.
 *
 * Naming:
 *   PGY_CODE_*   — stable diagnostic code (mirrors string in docs/72)
 *   PGY_CAUSE_*  — cause_ir routing tag (IR-level origin)
 *   PGY_FIX_*    — fix_source action token (source-level repair)
 */

#ifndef PGY_DIAG_CODES_H
#define PGY_DIAG_CODES_H

/* =================================================================
 * Stable diagnostic codes
 *
 * Documented in docs/72_diagnostic_codes.md.  Codes are grouped by
 * stage prefix for downstream stage routing (driver_route_stage).
 * ================================================================= */

/* --- Semantic (PGY_SEM_*) --- */
#define PGY_CODE_SEM_TYPE_MISMATCH              "PGY_SEM_TYPE_MISMATCH"
#define PGY_CODE_SEM_BINOP_TYPE_MISMATCH        "PGY_SEM_BINOP_TYPE_MISMATCH"
#define PGY_CODE_SEM_UNOP_TYPE_MISMATCH         "PGY_SEM_UNOP_TYPE_MISMATCH"
#define PGY_CODE_SEM_UNKNOWN_TYPE               "PGY_SEM_UNKNOWN_TYPE"
#define PGY_CODE_SEM_UNDEFINED_SYMBOL           "PGY_SEM_UNDEFINED_SYMBOL"
#define PGY_CODE_SEM_INFER_COLLECTION           "PGY_SEM_INFER_COLLECTION"
#define PGY_CODE_SEM_INFER_GENERIC              "PGY_SEM_INFER_GENERIC"
#define PGY_CODE_SEM_INFER_REQUIRED             "PGY_SEM_INFER_REQUIRED"
#define PGY_CODE_SEM_SLOT_RELEASED              "PGY_SEM_SLOT_RELEASED"
#define PGY_CODE_SEM_RELEASE_REQUIRES_OWNER     "PGY_SEM_RELEASE_REQUIRES_OWNER"
#define PGY_CODE_SEM_SLOT_DOUBLE_RELEASE        "PGY_SEM_SLOT_DOUBLE_RELEASE"
#define PGY_CODE_SEM_VIEW_KIND_MISMATCH         "PGY_SEM_VIEW_KIND_MISMATCH"
#define PGY_CODE_SEM_MOVE_TOKEN_MISUSE          "PGY_SEM_MOVE_TOKEN_MISUSE"
#define PGY_CODE_SEM_MOVE_FROM_RELEASED         "PGY_SEM_MOVE_FROM_RELEASED"
#define PGY_CODE_SEM_PARALLEL_SLOT_CONFLICT     "PGY_SEM_PARALLEL_SLOT_CONFLICT"
#define PGY_CODE_SEM_PARALLEL_SLOT_RACE_RISK    "PGY_SEM_PARALLEL_SLOT_RACE_RISK"
#define PGY_CODE_SEM_EFFECT_CONFLICT            "PGY_SEM_EFFECT_CONFLICT"
#define PGY_CODE_SEM_PARALLEL_SECURE_FORBIDDEN  "PGY_SEM_PARALLEL_SECURE_FORBIDDEN"
#define PGY_CODE_SEM_REDECLARATION              "PGY_SEM_REDECLARATION"
#define PGY_CODE_SEM_BORROW_ESCAPE              "PGY_SEM_BORROW_ESCAPE"
#define PGY_CODE_SEM_INTENT_STEP_INVALID        "PGY_SEM_INTENT_STEP_INVALID"
#define PGY_CODE_SEM_ACTION_CONTRACT_INVALID    "PGY_SEM_ACTION_CONTRACT_INVALID"
#define PGY_CODE_SEM_ABILITY_CONTRACT_INVALID   "PGY_SEM_ABILITY_CONTRACT_INVALID"
#define PGY_CODE_SEM_ROLE_CONTRACT_INVALID      "PGY_SEM_ROLE_CONTRACT_INVALID"
#define PGY_CODE_SEM_CLASS_CONTRACT_INVALID     "PGY_SEM_CLASS_CONTRACT_INVALID"
#define PGY_CODE_SEM_ZONE_CONTRACT_INVALID      "PGY_SEM_ZONE_CONTRACT_INVALID"
#define PGY_CODE_SEM_WORLD_CONTRACT_INVALID     "PGY_SEM_WORLD_CONTRACT_INVALID"
#define PGY_CODE_SEM_LOOP_CONTROL_INVALID       "PGY_SEM_LOOP_CONTROL_INVALID"
#define PGY_CODE_SEM_BUILTIN_ARGS_INVALID       "PGY_SEM_BUILTIN_ARGS_INVALID"
#define PGY_CODE_SEM_PREDICATE_ARGS_INVALID     "PGY_SEM_PREDICATE_ARGS_INVALID"
#define PGY_CODE_SEM_EVENT_CONTRACT_INVALID     "PGY_SEM_EVENT_CONTRACT_INVALID"
#define PGY_CODE_SEM_REMOTE_FUTURE_MISUSE       "PGY_SEM_REMOTE_FUTURE_MISUSE"
#define PGY_CODE_SEM_ANCHORED_HANDLE_COPY       "PGY_SEM_ANCHORED_HANDLE_COPY"
#define PGY_CODE_SEM_TYPE_DEPENDENCY_CYCLE      "PGY_SEM_TYPE_DEPENDENCY_CYCLE"
#define PGY_CODE_SEM_MATCH_PATTERN_INVALID      "PGY_SEM_MATCH_PATTERN_INVALID"
#define PGY_CODE_SEM_SELECT_CASE_INVALID        "PGY_SEM_SELECT_CASE_INVALID"
#define PGY_CODE_SEM_VISIBILITY_BOUNDARY        "PGY_SEM_VISIBILITY_BOUNDARY"
#define PGY_CODE_SEM_IMMUTABLE_FIELD_WRITE      "PGY_SEM_IMMUTABLE_FIELD_WRITE"
#define PGY_CODE_SEM_CHANNEL_TRANSPORT_INVALID  "PGY_SEM_CHANNEL_TRANSPORT_INVALID"

/* --- MIR (PGY_MIR_*) --- */
#define PGY_CODE_MIR_INTENT_CARRIER_MISSING     "PGY_MIR_INTENT_CARRIER_MISSING"
#define PGY_CODE_MIR_SIGNATURE_UNSUPPORTED      "PGY_MIR_SIGNATURE_UNSUPPORTED"
#define PGY_CODE_MIR_SSA_LIMIT                  "PGY_MIR_SSA_LIMIT"
#define PGY_CODE_MIR_TOPOLOGY_INVALID           "PGY_MIR_TOPOLOGY_INVALID"
#define PGY_CODE_MIR_UNRESOLVED_LOCAL           "PGY_MIR_UNRESOLVED_LOCAL"

/* --- LLVM backend (PGY_LLVM_*) --- */
#define PGY_CODE_LLVM_OOM                       "PGY_LLVM_OOM"
#define PGY_CODE_LLVM_SCOPE_LIMIT               "PGY_LLVM_SCOPE_LIMIT"
#define PGY_CODE_LLVM_SPEC_LIMIT                "PGY_LLVM_SPEC_LIMIT"
#define PGY_CODE_LLVM_TYPE_UNSUPPORTED          "PGY_LLVM_TYPE_UNSUPPORTED"
#define PGY_CODE_LLVM_MIR_ROUTINE_MISSING       "PGY_LLVM_MIR_ROUTINE_MISSING"

/* --- C backend (PGY_C_*) --- */
#define PGY_CODE_C_TYPE_UNSUPPORTED             "PGY_C_TYPE_UNSUPPORTED"

/* =================================================================
 * cause_ir routing tags
 *
 * Namespace: <stage>:<area>:<specific>
 * Stages: semantic, mir, llvm, c, parse, lex, io
 * ================================================================= */

/* --- Semantic: contracts --- */
#define PGY_CAUSE_ABILITY_CONTRACT              "semantic:ability_contract"
#define PGY_CAUSE_ACTION_CONTRACT               "semantic:action_contract"
#define PGY_CAUSE_CLASS_CONTRACT                "semantic:class_contract"
#define PGY_CAUSE_ROLE_CONTRACT                 "semantic:role_contract"
#define PGY_CAUSE_ZONE_CONTRACT                 "semantic:zone_contract"
#define PGY_CAUSE_WORLD_CONTRACT                "semantic:world_contract"

/* --- Semantic: types --- */
#define PGY_CAUSE_TYPE_UNKNOWN                  "semantic:type:unknown"
#define PGY_CAUSE_TYPE_RESOLUTION_CYCLE         "semantic:type_resolution:cycle"
#define PGY_CAUSE_ASSIGNABILITY_CHECK           "semantic:assignability_check"
#define PGY_CAUSE_TYPE_MOVABLE_HANDLE_REQUIRED  "semantic:type:movable_handle_required"
#define PGY_CAUSE_TYPE_RESOURCE_HANDLE_ARG_MISMATCH \
                                                "semantic:type:resource_handle_arg_mismatch"
#define PGY_CAUSE_TYPE_SUBJECT_ARG_MISMATCH     "semantic:type:subject_arg_mismatch"
#define PGY_CAUSE_TYPE_BOUNDARY_ARG_MISMATCH    "semantic:type:boundary_arg_mismatch"

/* --- Semantic: slot lifecycle --- */
#define PGY_CAUSE_SLOT_LIFECYCLE_READ_AFTER_RELEASE \
                                                "semantic:slot_lifecycle:read_after_release"
#define PGY_CAUSE_SLOT_LIFECYCLE_WRITE_AFTER_RELEASE \
                                                "semantic:slot_lifecycle:write_after_release"
#define PGY_CAUSE_SLOT_VIEW_READ_THROUGH_RELEASED_OWNER \
                                                "semantic:slot_view:read_through_released_owner"
#define PGY_CAUSE_SLOT_VIEW_WRITE_THROUGH_RELEASED_OWNER \
                                                "semantic:slot_view:write_through_released_owner"
#define PGY_CAUSE_SLOT_BORROW_RELEASED          "semantic:slot:borrow_released"
#define PGY_CAUSE_SLOT_WRITE_VALUE_TYPE_MISMATCH "semantic:slot_write:value_type_mismatch"
#define PGY_CAUSE_RELEASE_DOUBLE                "semantic:release:double"
#define PGY_CAUSE_RELEASE_NON_OWNING_RECEIVER   "semantic:release:non_owning_receiver"
#define PGY_CAUSE_DEVICE_SLOT_USE_AFTER_RELEASE "semantic:device_slot:use_after_release"
#define PGY_CAUSE_VIEW_KIND_OP_MISMATCH         "semantic:view_kind:op_mismatch"

/* --- Semantic: ownership / move / handles --- */
#define PGY_CAUSE_BORROW_ESCAPE                 "semantic:borrow_escape"
#define PGY_CAUSE_MOVE_ONLY_ASSIGNMENT_REBIND   "semantic:move_only:assignment_rebind"
#define PGY_CAUSE_MOVE_FROM_RELEASED            "semantic:move:from_released"
#define PGY_CAUSE_MOVE_SOURCE_NOT_NAMED         "semantic:move:source_not_named"
#define PGY_CAUSE_MOVE_TOKEN_DIRECT_ACCESS      "semantic:move_token:direct_access"
#define PGY_CAUSE_ANCHORED_HANDLE_COPY_ATTEMPT  "semantic:anchored_handle:copy_attempt"
#define PGY_CAUSE_ANCHORED_HANDLE_RETURN_BOUNDARY \
                                                "semantic:anchored_handle:return_boundary"
#define PGY_CAUSE_MOVABLE_HANDLE_COPY_ATTEMPT   "semantic:movable_handle:copy_attempt"

/* --- Semantic: channels --- */
#define PGY_CAUSE_CHANNEL_TRANSPORT_RULE_VIOLATION \
                                                "semantic:channel:transport_rule_violation"

/* --- Semantic: builtins / predicates --- */
#define PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH    "semantic:builtin:signature_mismatch"
#define PGY_CAUSE_BUILTIN_SLOT_TYPE_REQUIRED    "semantic:builtin:slot_type_required"
#define PGY_CAUSE_BUILTIN_CAPACITY_NON_INTEGER  "semantic:builtin:capacity_non_integer"
#define PGY_CAUSE_PREDICATE_ARGS                "semantic:predicate:args"

/* --- Semantic: generics --- */
#define PGY_CAUSE_GENERIC_ARGS_INVALID          "semantic:generic:args_invalid"
#define PGY_CAUSE_GENERIC_BOUND_VALIDATION_FAILED \
                                                "semantic:generic:bound_validation_failed"
#define PGY_CAUSE_GENERIC_DEFAULT_UNRESOLVED    "semantic:generic:default_unresolved"
#define PGY_CAUSE_GENERIC_NON_TRAILING_DEFAULT  "semantic:generic:non_trailing_default"
#define PGY_CAUSE_INFER_UNBOUND_GENERIC         "semantic:infer:unbound_generic"
#define PGY_CAUSE_INFER_NO_SOURCE               "semantic:infer:no_source"
#define PGY_CAUSE_INFER_COLLECTION_NEEDS_ANNOTATION \
                                                "semantic:infer:collection_needs_annotation"

/* --- Semantic: control flow / patterns --- */
#define PGY_CAUSE_CONDITION_NON_BOOL            "semantic:condition:non_bool"
#define PGY_CAUSE_LOOP_CONTROL                  "semantic:loop_control"
#define PGY_CAUSE_FOR_IN_NON_ITERABLE           "semantic:for_in:non_iterable"
#define PGY_CAUSE_MATCH_PATTERN_SHAPE           "semantic:match:pattern_shape"
#define PGY_CAUSE_SELECT_CASE_SHAPE             "semantic:select:case_shape"
#define PGY_CAUSE_DESTRUCTURING_ARITY_MISMATCH  "semantic:destructuring:arity_mismatch"
#define PGY_CAUSE_BINOP_OPERAND_TYPES           "semantic:binop:operand_types"
#define PGY_CAUSE_UNARY_OPERATOR_OPERAND        "semantic:unary_operator:operand"

/* --- Semantic: scopes / symbols --- */
#define PGY_CAUSE_SYMBOL_UNDEFINED              "semantic:symbol:undefined"
#define PGY_CAUSE_SCOPE_DUPLICATE_SYMBOL        "semantic:scope:duplicate_symbol"
#define PGY_CAUSE_VISIBILITY_BOUNDARY_CROSS     "semantic:visibility:boundary_cross"
#define PGY_CAUSE_IMMUTABLE_FIELD_WRITE         "semantic:immutable_field:write"
#define PGY_CAUSE_RESOLUTION_OOM                "semantic:resolution:oom"

/* --- Semantic: parallel / async --- */
#define PGY_CAUSE_PARALLEL_SECURE_IN_TASK       "semantic:parallel:secure_in_task"
#define PGY_CAUSE_ASYNC_CONTEXT_REQUIRED        "semantic:async:context_required"
#define PGY_CAUSE_AWAIT_NON_FUTURE              "semantic:await:non_future"
#define PGY_CAUSE_REMOTE_FUTURE_DIRECT_ACCESS   "semantic:remote_future:direct_access"

/* --- Semantic: intent / event / domain --- */
#define PGY_CAUSE_INTENT_STEP                   "semantic:intent_step"
#define PGY_CAUSE_INTENT_NON_BOOL_CLAUSE        "semantic:intent:non_bool_clause"
#define PGY_CAUSE_INTENT_PRIORITY_NON_INT       "semantic:intent:priority_non_int"
#define PGY_CAUSE_EVENT_SIGNATURE               "semantic:event:signature"
#define PGY_CAUSE_DOMAIN_VESSEL_REQUIRED        "semantic:domain:vessel_required"
#define PGY_CAUSE_EFFECT_INCOMPATIBLE_COMBO     "semantic:effect:incompatible_combo"
#define PGY_CAUSE_SUBJECT_REBIND_FORBIDDEN      "semantic:subject:rebind_forbidden"

/* --- Semantic: arrays / containers --- */
#define PGY_CAUSE_ARRAY_ACCESS_INDEX_NON_INT    "semantic:array_access:index_non_int"
#define PGY_CAUSE_ARRAY_ACCESS_TARGET_NOT_INDEXABLE \
                                                "semantic:array_access:target_not_indexable"
#define PGY_CAUSE_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH \
                                                "semantic:array_literal:element_type_mismatch"

/* --- Semantic: function call shape --- */
#define PGY_CAUSE_CALL_NOT_CALLABLE             "semantic:call:not_callable"
#define PGY_CAUSE_CALL_ARG_TYPE_MISMATCH        "semantic:call:arg_type_mismatch"
#define PGY_CAUSE_SLOT_PARAM_QUALIFIER_MISSING  "semantic:slot_param:qualifier_missing"

/* --- Semantic: declaration duplicates --- */
#define PGY_CAUSE_ABILITY_DUPLICATE_NAME        "semantic:ability:duplicate_name"
#define PGY_CAUSE_CLASS_DUPLICATE_NAME          "semantic:class:duplicate_name"
#define PGY_CAUSE_DOMAIN_SLOT_DUPLICATE_NAME    "semantic:domain_slot:duplicate_name"
#define PGY_CAUSE_FUNCTION_DUPLICATE_NAME       "semantic:function:duplicate_name"
#define PGY_CAUSE_INTENT_DUPLICATE_NAME         "semantic:intent:duplicate_name"
#define PGY_CAUSE_OVERLAY_DUPLICATE_NAME        "semantic:overlay:duplicate_name"
#define PGY_CAUSE_PARTY_DUPLICATE_NAME          "semantic:party:duplicate_name"
#define PGY_CAUSE_ROLE_DUPLICATE_NAME           "semantic:role:duplicate_name"
#define PGY_CAUSE_ROSTER_DUPLICATE_NAME         "semantic:roster:duplicate_name"
#define PGY_CAUSE_WORLD_DUPLICATE_NAME          "semantic:world:duplicate_name"
#define PGY_CAUSE_WORLD_STATE_DUPLICATE_NAME    "semantic:world_state:duplicate_name"
#define PGY_CAUSE_ZONE_STATE_DUPLICATE_NAME     "semantic:zone_state:duplicate_name"

/* --- Semantic: parameter modes --- */
#define PGY_CAUSE_PARAM_MODE_UNSUPPORTED_BOUNDARY_TYPE \
                                                "semantic:param_mode:unsupported_boundary_type"

/* --- MIR --- */
#define PGY_CAUSE_MIR_INTENT_CARRIER_MISSING    "mir:intent:carrier_missing"
#define PGY_CAUSE_MIR_LOCAL_UNRESOLVED          "mir:local:unresolved"
#define PGY_CAUSE_MIR_SIGNATURE_UNSUPPORTED     "mir:signature:unsupported"
#define PGY_CAUSE_MIR_SSA_CAPACITY_EXCEEDED     "mir:ssa:capacity_exceeded"
#define PGY_CAUSE_MIR_TOPOLOGY_INVALID          "mir:topology:invalid"
#define PGY_CAUSE_MIR_TOPOLOGY_ROUTINE_MISSING  "mir:topology:routine_missing_or_malformed"

/* --- C backend --- */
#define PGY_CAUSE_C_TYPE_UNSUPPORTED            "c_codegen:type:unsupported"
#define PGY_CAUSE_C_MIR_TOPOLOGY_INVALID        "c_codegen:mir:topology_invalid"

/* --- LLVM --- */
#define PGY_CAUSE_LLVM_MEMORY_EXHAUSTED         "llvm:memory:exhausted"
#define PGY_CAUSE_LLVM_MIR_ROUTINE_MISSING      "llvm:mir:routine_missing"
#define PGY_CAUSE_LLVM_RESULT_SPEC_CAPACITY     "llvm:result_spec:capacity_exceeded"
#define PGY_CAUSE_LLVM_SCOPE_CAPACITY           "llvm:scope:capacity_exceeded"
#define PGY_CAUSE_LLVM_SLOT_BINDING_MISSING     "llvm:slot:binding_missing"
#define PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING  "llvm:slot:inner_type_missing"
#define PGY_CAUSE_LLVM_TYPE_UNSUPPORTED         "llvm:type:unsupported_or_unknown"

/* =================================================================
 * fix_source action tokens
 *
 * Verb-first kebab-case action tokens.  These are stable enough that
 * tooling (LSP quick-fix, AI router, doc cross-link) can dispatch on
 * them.  Adding a new fix MUST go through this header and docs/72.
 * ================================================================= */

/* --- align-* (re-shape something) --- */
#define PGY_FIX_ACQUIRE_MATCHING_VIEW_OR_USE_SLOT \
                                                "acquire-matching-view-or-use-slot"
#define PGY_FIX_ALIGN_ABILITY_GENERICS_OR_FIELDS \
                                                "align-ability-generics-or-fields"
#define PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE  "align-action-surface-with-zone"
#define PGY_FIX_ALIGN_ARG_TYPE                  "align-arg-type"
#define PGY_FIX_ALIGN_ARRAY_ELEMENT_TYPES       "align-array-element-types"
#define PGY_FIX_ALIGN_BOUNDARY_ARG_TYPE         "align-boundary-arg-type"
#define PGY_FIX_ALIGN_CHANNEL_ELEMENT_TYPE      "align-channel-element-type"
#define PGY_FIX_ALIGN_DESTRUCTURING_ARITY       "align-destructuring-arity"
#define PGY_FIX_ALIGN_EVENT_SIGNATURE           "align-event-signature"
#define PGY_FIX_ALIGN_GENERIC_ARG_LIST          "align-generic-arg-list"
#define PGY_FIX_ALIGN_GENERIC_BOUND_OR_ANNOTATE "align-generic-bound-or-annotate"
#define PGY_FIX_ALIGN_OPERAND_TYPE              "align-operand-type"
#define PGY_FIX_ALIGN_OPERAND_TYPES_OR_OVERLOAD "align-operand-types-or-overload"
#define PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND     "align-pattern-arity-or-kind"
#define PGY_FIX_ALIGN_RESOURCE_HANDLE_ARG       "align-resource-handle-arg"
#define PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY    "align-role-impl-with-ability"
#define PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS \
                                                "align-step-with-zone-action-contracts"
#define PGY_FIX_ALIGN_SUBJECT_ARG_TYPE          "align-subject-arg-type"
#define PGY_FIX_ALIGN_VALUE_TO_SLOT_INNER       "align-value-to-slot-inner"
#define PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION \
                                                "align-world-zone-state-composition"
#define PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING "align-zone-slot-or-state-naming"

/* --- annotate-* (add type/binding info) --- */
#define PGY_FIX_ANNOTATE_BINDING_TYPE           "annotate-binding-type"
#define PGY_FIX_ANNOTATE_COLLECTION_ELEMENT_TYPE \
                                                "annotate-collection-element-type"
#define PGY_FIX_ANNOTATE_CONCRETE_TYPE          "annotate-concrete-type"
#define PGY_FIX_ANNOTATE_OR_CONVERT             "annotate-or-convert"
#define PGY_FIX_ANNOTATE_SLOT_PARAM_QUALIFIER   "annotate-slot-param-qualifier"

/* --- bind / convert / declare / use / pass --- */
#define PGY_FIX_ADD_ANNOTATION_OR_INITIALIZER   "add-annotation-or-initializer"
#define PGY_FIX_BIND_THE_MOVED_VALUE_ONCE       "bind-the-moved-value-once"
#define PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE \
                                                "bind-to-named-variable-before-move"
#define PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_SEND \
                                                "bind-to-named-variable-before-send"
#define PGY_FIX_CONVERT_CONDITION_TO_BOOL       "convert-condition-to-bool"
#define PGY_FIX_CONVERT_TO_BOOL                 "convert-to-bool"
#define PGY_FIX_DECLARE_OR_IMPORT_TYPE          "declare-or-import-type"
#define PGY_FIX_DECLARE_VESSEL_TYPE             "declare-vessel-type"
#define PGY_FIX_IMPORT_OR_DECLARE_SYMBOL        "import-or-declare-symbol"
#define PGY_FIX_IMPORT_OR_DECLARE_TYPE          "import-or-declare-type"
#define PGY_FIX_USE_ARRAY_OR_SLICE              "use-array-or-slice"
#define PGY_FIX_USE_ARRAY_SLICE_OR_LIST         "use-array-slice-or-list"
#define PGY_FIX_USE_BOUNDARY_VISIBLE_TYPE_OR_DROP_QUALIFIER \
                                                "use-boundary-visible-type-or-drop-qualifier"
#define PGY_FIX_USE_CALLABLE_DECLARATION        "use-callable-declaration"
#define PGY_FIX_USE_INT_INDEX                   "use-int-index"
#define PGY_FIX_USE_INT_OR_LONG_CAPACITY        "use-int-or-long-capacity"
#define PGY_FIX_USE_INT_PRIORITY                "use-int-priority"
#define PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER \
                                                "use-llvm-backend-or-extend-transpiler"
#define PGY_FIX_USE_MOVE_OR_RETAIN_BINDING      "use-move-or-retain-binding"
#define PGY_FIX_USE_OWN_SECURE_SLOT             "use-own-secure-slot"
#define PGY_FIX_USE_SLOT_BOUND_IDENTIFIER       "use-slot-bound-identifier"
#define PGY_FIX_USE_SUPPORTED_PARAM_TYPE        "use-supported-param-type"
#define PGY_FIX_PASS_DEVICE_SLOT                "pass-device-slot"
#define PGY_FIX_PASS_OWNING_SLOT                "pass-owning-slot"
#define PGY_FIX_PROVIDE_MOVABLE_HANDLE          "provide-movable-handle"
#define PGY_FIX_RENAME_OR_REMOVE_DUPLICATE      "rename-or-remove-duplicate"

/* --- keep / move / mutate --- */
#define PGY_FIX_KEEP_HANDLE_LOCAL_OR_PROJECT    "keep-handle-local-or-project"
#define PGY_FIX_KEEP_HANDLE_LOCAL_OR_SEND_INNER_VALUE \
                                                "keep-handle-local-or-send-inner-value"
#define PGY_FIX_MOVE_DEFAULTS_TO_TRAILING       "move-defaults-to-trailing"
#define PGY_FIX_MOVE_INTO_ASYNC_FUNCTION        "move-into-async-function"
#define PGY_FIX_MOVE_INTO_LOOP_OR_FIX_LABEL     "move-into-loop-or-fix-label"
#define PGY_FIX_MUTATE_FIELD_OR_USE_METHOD      "mutate-field-or-use-method"

/* --- reclaim / release / return --- */
#define PGY_FIX_RECLAIM_BEFORE_USE              "reclaim-before-use"
#define PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE   "reclaim-or-trace-earlier-move"
#define PGY_FIX_RECLAIM_SOURCE_OR_DROP_VIEW     "reclaim-source-or-drop-view"
#define PGY_FIX_RECLAIM_SOURCE_OR_TRACE_EARLIER_RELEASE \
                                                "reclaim-source-or-trace-earlier-release"
#define PGY_FIX_RELEASE_OWNING_SLOT_NOT_VIEW    "release-owning-slot-not-view"
#define PGY_FIX_REMOVE_REDUNDANT_RELEASE        "remove-redundant-release"
#define PGY_FIX_RETURN_INNER_VALUE_OR_KEEP_LOCAL \
                                                "return-inner-value-or-keep-local"
#define PGY_FIX_RETURN_PROJECTION_OR_KEEP_LOCAL "return-projection-or-keep-local"

/* --- meta / structural --- */
#define PGY_FIX_AWAIT_FUTURE                    "await-future"
#define PGY_FIX_AWAIT_FUTURE_TYPE               "await-future-type"
#define PGY_FIX_BREAK_CYCLE_VIA_INDIRECTION     "break-cycle-via-indirection"
#define PGY_FIX_CHANGE_REF_TO_OWN_OR_STOP_ESCAPE \
                                                "change-ref-to-own-or-stop-escape"
#define PGY_FIX_CHECK_INTENT_STEP_LOWERING      "check-intent-step-lowering"
#define PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING     "inspect-hir-to-mir-lowering"
#define PGY_FIX_INSPECT_MIR_INVENTORY           "inspect-mir-inventory"
#define PGY_FIX_MATCH_BUILTIN_SIGNATURE         "match-builtin-signature"
#define PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST \
                                                "match-predicate-signature-in-host"
#define PGY_FIX_MATERIALIZE_TOKEN_TO_SLOT       "materialize-token-to-slot"
#define PGY_FIX_RECONSTRUCT_OR_CHANGE_HOST_KIND "reconstruct-or-change-host-kind"
#define PGY_FIX_REDUCE_SCOPE_OR_RETRY           "reduce-scope-or-retry"
#define PGY_FIX_REDUCE_UNIT_SIZE_OR_RAISE_LIMIT "reduce-unit-size-or-raise-limit"
#define PGY_FIX_REFACTOR_OR_RAISE_LIMIT         "refactor-or-raise-limit"
#define PGY_FIX_REPORT_COMPILER_BUG             "report-compiler-bug"
#define PGY_FIX_REUSE_SHARED_ERROR_ENUM         "reuse-shared-error-enum"
#define PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN  "satisfy-generic-bound-or-widen"
#define PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL      "serialize-outside-parallel"
#define PGY_FIX_SIMPLIFY_FUNCTION_SIGNATURE     "simplify-function-signature"
#define PGY_FIX_SPLIT_EFFECT_FAMILIES           "split-effect-families"
#define PGY_FIX_START_WITH_CHANNEL_RECV         "start-with-channel-recv"
#define PGY_FIX_WIDEN_VISIBILITY_OR_MOVE_CALLER "widen-visibility-or-move-caller"

#endif /* PGY_DIAG_CODES_H */

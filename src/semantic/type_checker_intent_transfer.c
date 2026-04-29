#include "type_checker_internal.h"
#include "type_checker_intent_helpers_internal.h"
#include "diag_codes.h"

#include <string.h>

static Type *
intent_transfer_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    Type *resolved = semantic_type_resolution_lookup_metadata_type_ref(ctx, type_ref);
    return resolved != NULL ? resolved : TYPE_UNKNOWN;
}

static Type *
intent_transfer_resolve_involves_type(ASTNode *involves, SemanticContext *ctx)
{
    ASTNode *type_ref;
    if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
        return TYPE_UNKNOWN;
    type_ref = involves->data.intent_involves.subject_type;
    return intent_transfer_resolve_type_ref(type_ref, ctx);
}

static Type *
intent_transfer_resolve_step_where_type(ASTNode *step, SemanticContext *ctx)
{
    ASTNode *type_ref;
    if (step == NULL || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL) {
        return NULL;
    }
    type_ref = step->data.intent_step.where_type;
    return intent_transfer_resolve_type_ref(type_ref, ctx);
}

void
type_check_intent_step_transfer_contract(ASTNode *node,
                                         ASTNode *step,
                                         SemanticContext *ctx)
{
    if (step->data.intent_step.transfer_from_alias != NULL
            || step->data.intent_step.transfer_to_alias != NULL) {
            const char *from_alias = step->data.intent_step.transfer_from_alias;
            const char *to_alias = NULL;
            ASTNode *from_involves = find_intent_involves_local(node, from_alias);
            ASTNode *to_involves = intent_step_resolve_transfer_target_involves(
                node, step, &to_alias);
            Type *from_type = from_involves != NULL
                ? intent_transfer_resolve_involves_type(from_involves, ctx) : NULL;
            Type *to_type = to_involves != NULL
                ? intent_transfer_resolve_involves_type(to_involves, ctx) : NULL;
            ASTNode *from_zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                from_type != NULL ? from_type->name : NULL);
            ASTNode *to_zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                to_type != NULL ? to_type->name : NULL);
            Type *where_zone_type = intent_transfer_resolve_step_where_type(step, ctx);

            if (from_alias == NULL || to_alias == NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                    "Intent step '%s' transfer clause requires both source and target zone aliases.\n"
                    "Reason:\n"
                    "- transfer declares a zone handoff contract\n"
                    "Contract source:\n"
                    "- the step-local transfer clause itself\n"
                    "- handoff materialization cannot proceed without both source and target bindings\n"
                    "Fix:\n"
                    "- provide 'transfer: <sourceZoneAlias> -> <targetZoneAlias>;'\n"
                    "- or remove the incomplete transfer clause",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            }
            if (from_alias != NULL && from_involves == NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                    "Intent step '%s' transfer source '%s' is not a declared intent binding.\n"
                    "Reason:\n"
                    "Contract source:\n"
                    "- transfer handoff edge '%s' -> '%s'\n"
                    "- the handoff source must name an intent binding that participates in this orchestration\n"
                    "- no declared binding named '%s' was found on the current intent\n"
                    "Fix:\n"
                    "- use a declared zone binding as the transfer source\n"
                    "- or add '%s' as an intent binding before using it in transfer",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    from_alias,
                    from_alias,
                    to_alias != NULL ? to_alias : "<target>",
                    from_alias,
                    from_alias);
            }
            if (to_alias != NULL && to_involves == NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                    "Intent step '%s' transfer target '%s' is not a declared intent binding or a unique zone type in this intent.\n"
                    "Reason:\n"
                    "Contract source:\n"
                    "- transfer handoff edge '%s' -> '%s'\n"
                    "- transfer target resolution first looks for an explicit binding alias\n"
                    "- then it allows a unique zone-type shorthand only when that target is unambiguous\n"
                    "- current intent provides no unique target for '%s'\n"
                    "Fix:\n"
                    "- use a declared zone binding alias as the transfer target\n"
                    "- or make the target zone type unique within this intent",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    to_alias,
                    from_alias != NULL ? from_alias : "<source>",
                    to_alias,
                    to_alias);
            }
            if (from_involves != NULL && from_zone_decl == NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                    "Intent step '%s' transfer source '%s' must be a zone binding.\n"
                    "Reason:\n"
                    "Contract source:\n"
                    "- transfer handoff edge '%s' -> '%s'\n"
                    "- transfer performs zone-to-zone handoff\n"
                    "- source binding '%s' does not resolve to a zone declaration\n"
                    "Fix:\n"
                    "- use a zone binding as the transfer source\n"
                    "- or move ordinary participant movement into 'who'/'on' instead of 'transfer'",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    from_alias != NULL ? from_alias : "<source>",
                    from_alias != NULL ? from_alias : "<source>",
                    to_alias != NULL ? to_alias : "<target>",
                    from_alias != NULL ? from_alias : "<source>");
            }
            if (to_involves != NULL && to_zone_decl == NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                    "Intent step '%s' transfer target '%s' must be a zone binding.\n"
                    "Reason:\n"
                    "Contract source:\n"
                    "- transfer handoff edge '%s' -> '%s'\n"
                    "- transfer materializes handoff into a target zone\n"
                    "- target binding '%s' does not resolve to a zone declaration\n"
                    "Fix:\n"
                    "- use a zone binding as the transfer target\n"
                    "- or change the step contract so the handoff target is a declared zone",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    to_alias != NULL ? to_alias : "<target>",
                    from_alias != NULL ? from_alias : "<source>",
                    to_alias != NULL ? to_alias : "<target>",
                    to_alias != NULL ? to_alias : "<target>");
            }
            if (to_type != NULL && where_zone_type != NULL && !type_equals(to_type, where_zone_type)) {
                char contract_summary[512];
                intent_step_format_contract_source_summary(
                    node, step, ctx, contract_summary, sizeof(contract_summary));
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                    "Intent step '%s' transfer target '%s' does not match the current zone contract.\n"
                    "Reason:\n"
                    "Contract source:\n"
                    "- transfer handoff edge '%s' -> '%s'\n"
                    "- transfer target '%s' has zone type '%s'\n"
                    "- current step zone is '%s'\n"
                    "- transfer target derivation would choose zone '%s'\n"
                    "%s- current zone contract provenance is:\n"
                    "%s%s"
                    "Fix:\n"
                    "- change the transfer target to a '%s' binding\n"
                    "- or override the step zone to '%s'",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    to_alias != NULL ? to_alias : "<target>",
                    from_alias != NULL ? from_alias : "<source>",
                    to_alias != NULL ? to_alias : "<target>",
                    to_alias != NULL ? to_alias : "<target>",
                    to_type->name != NULL ? to_type->name : "<zone>",
                    where_zone_type->name != NULL ? where_zone_type->name : "<zone>",
                    to_type->name != NULL ? to_type->name : "<zone>",
                    contract_summary[0] != '\0' ? "" : "- no additional step contract provenance was recorded\n",
                    contract_summary[0] != '\0' ? contract_summary : "",
                    contract_summary[0] != '\0' ? "\n" : "",
                    where_zone_type->name != NULL ? where_zone_type->name : "<zone>",
                    to_type->name != NULL ? to_type->name : "<zone>");
            }
            if (step->data.intent_step.using_expr != NULL
                && step->data.intent_step.using_expr->type == AST_IDENTIFIER
                && to_alias != NULL
                && strcmp(step->data.intent_step.using_expr->data.identifier.name, to_alias) != 0) {
                char contract_summary[512];
                intent_step_format_contract_source_summary(
                    node, step, ctx, contract_summary, sizeof(contract_summary));
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step->data.intent_step.using_expr,
                    "Intent step '%s' using binding does not match the transfer target.\n"
                    "Reason:\n"
                    "Contract source:\n"
                    "- transfer handoff edge '%s' -> '%s'\n"
                    "- current using binding is '%s'\n"
                    "- transfer target binding is '%s'\n"
                    "- transfer target derivation would choose using '%s'\n"
                    "%s- current handoff provenance is:\n"
                    "%s%s"
                    "Fix:\n"
                    "- change 'using' to '%s'\n"
                    "- or change the transfer target to '%s'",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    from_alias != NULL ? from_alias : "<source>",
                    to_alias != NULL ? to_alias : "<target>",
                    step->data.intent_step.using_expr->data.identifier.name != NULL
                        ? step->data.intent_step.using_expr->data.identifier.name : "<binding>",
                    to_alias,
                    to_alias,
                    to_alias,
                    contract_summary[0] != '\0' ? "" : "- no additional step contract provenance was recorded\n",
                    contract_summary[0] != '\0' ? contract_summary : "",
                    contract_summary[0] != '\0' ? "\n" : "",
                    step->data.intent_step.using_expr->data.identifier.name != NULL
                        ? step->data.intent_step.using_expr->data.identifier.name : "<binding>");
            }
        }

}

#include "type_checker_internal.h"
#include "type_checker_intent_helpers_internal.h"
#include "diag_codes.h"

#include <string.h>

void
type_check_intent_step_transfer_contract(ASTNode *node,
                                         ASTNode *step,
                                         SemanticContext *ctx)
{
    const char *step_name;
    const char *from_alias;
    ASTNode *using_expr;

    if (node == NULL || step == NULL || step->type != AST_INTENT_STEP
        || ctx == NULL) {
        return;
    }
    step_name = ast_intent_step_name(step);
    from_alias = ast_intent_step_transfer_from_alias(step);
    if (from_alias == NULL && ast_intent_step_transfer_to_alias(step) == NULL) {
        return;
    }

    const char *to_alias = NULL;
    ASTNode *from_involves = find_intent_involves_local(node, from_alias);
    ASTNode *to_involves = intent_step_resolve_transfer_target_involves(
        node, step, &to_alias);
    Type *from_type = from_involves != NULL
        ? intent_resolve_involves_type(from_involves, ctx) : NULL;
    Type *to_type = to_involves != NULL
        ? intent_resolve_involves_type(to_involves, ctx) : NULL;
    ASTNode *from_zone_decl = intent_find_zone_decl_for_type(from_type, ctx);
    ASTNode *to_zone_decl = intent_find_zone_decl_for_type(to_type, ctx);
    Type *where_zone_type = intent_resolve_step_where_type(step, ctx);

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
            step_name != NULL ? step_name : "<step>");
    }
    if (from_alias != NULL && from_involves == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
            "Intent step '%s' transfer source '%s' is not a declared intent binding.\n"
            "Reason:\n"
            "- transfer source aliases must resolve before the handoff edge can be checked\n"
            "Contract source:\n"
            "- transfer handoff edge '%s' -> '%s'\n"
            "- the handoff source must name an intent binding that participates in this orchestration\n"
            "- no declared binding named '%s' was found on the current intent\n"
            "Fix:\n"
            "- use a declared zone binding as the transfer source\n"
            "- or add '%s' as an intent binding before using it in transfer",
            step_name != NULL ? step_name : "<step>",
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
            "- transfer target shorthand is only valid when it resolves to one zone binding\n"
            "Contract source:\n"
            "- transfer handoff edge '%s' -> '%s'\n"
            "- transfer target resolution first looks for an explicit binding alias\n"
            "- then it allows a unique zone-type shorthand only when that target is unambiguous\n"
            "- current intent provides no unique target for '%s'\n"
            "Fix:\n"
            "- use a declared zone binding alias as the transfer target\n"
            "- or make the target zone type unique within this intent",
            step_name != NULL ? step_name : "<step>",
            to_alias,
            from_alias != NULL ? from_alias : "<source>",
            to_alias,
            to_alias);
    }
    if (from_involves != NULL && from_zone_decl == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
            "Intent step '%s' transfer source '%s' must be a zone binding.\n"
            "Reason:\n"
            "- transfer handoff moves control between zones, not ordinary participants\n"
            "Contract source:\n"
            "- transfer handoff edge '%s' -> '%s'\n"
            "- transfer performs zone-to-zone handoff\n"
            "- source binding '%s' does not resolve to a zone declaration\n"
            "Fix:\n"
            "- use a zone binding as the transfer source\n"
            "- or move ordinary participant movement into 'who'/'on' instead of 'transfer'",
            step_name != NULL ? step_name : "<step>",
            from_alias != NULL ? from_alias : "<source>",
            from_alias != NULL ? from_alias : "<source>",
            to_alias != NULL ? to_alias : "<target>",
            from_alias != NULL ? from_alias : "<source>");
    }
    if (to_involves != NULL && to_zone_decl == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
            "Intent step '%s' transfer target '%s' must be a zone binding.\n"
            "Reason:\n"
            "- transfer target aliases must name the destination zone binding\n"
            "Contract source:\n"
            "- transfer handoff edge '%s' -> '%s'\n"
            "- transfer materializes handoff into a target zone\n"
            "- target binding '%s' does not resolve to a zone declaration\n"
            "Fix:\n"
            "- use a zone binding as the transfer target\n"
            "- or change the step contract so the handoff target is a declared zone",
            step_name != NULL ? step_name : "<step>",
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
            "- explicit or inherited step zone conflicts with the transfer destination\n"
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
            step_name != NULL ? step_name : "<step>",
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
    using_expr = ast_intent_step_using_expr(step);
    if (using_expr != NULL
        && using_expr->type == AST_IDENTIFIER
        && to_alias != NULL
        && strcmp(ast_identifier_name(using_expr), to_alias) != 0) {
        const char *using_name = ast_identifier_name(using_expr);
        char contract_summary[512];
        intent_step_format_contract_source_summary(
            node, step, ctx, contract_summary, sizeof(contract_summary));
        semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, using_expr,
            "Intent step '%s' using binding does not match the transfer target.\n"
            "Reason:\n"
            "- transfer target derivation and explicit using binding choose different zones\n"
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
            step_name != NULL ? step_name : "<step>",
            from_alias != NULL ? from_alias : "<source>",
            to_alias != NULL ? to_alias : "<target>",
            using_name != NULL ? using_name : "<binding>",
            to_alias,
            to_alias,
            to_alias,
            contract_summary[0] != '\0' ? "" : "- no additional step contract provenance was recorded\n",
            contract_summary[0] != '\0' ? contract_summary : "",
            contract_summary[0] != '\0' ? "\n" : "",
            using_name != NULL ? using_name : "<binding>");
    }
}

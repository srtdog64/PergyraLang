#include <string.h>

#include "diag_codes.h"
#include "parser/ast_api.h"
#include "type_checker_internal.h"
#include "type_checker_world_internal.h"

static const char *
world_state_name(ASTNode *state)
{
    const char *name;

    if (state == NULL || state->type != AST_WORLD_STATE)
        return "<unknown>";
    name = state->data.world_state.state_name;
    return name != NULL ? name : "<unknown>";
}

static void
check_composed_world_state(ASTNode *world,
                           ASTNode *state,
                           SemanticContext *ctx,
                           size_t state_index)
{
    bool saw_direct_zone_input = false;
    bool saw_state_input = false;

    if (state->data.world_state.input_count == 0) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_WORLD_CONTRACT_INVALID,
            PGY_CAUSE_WORLD_CONTRACT,
            PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION,
            state,
            "Composed world state '%s' must reference at least one zone/state input.\n"
            "Reason:\n"
            "- `all`/`any` world-state composition needs at least one upstream zone/state contract\n"
            "- an empty composition cannot produce observable world state\n"
            "Fix:\n"
            "- add one or more world zone/state inputs to '%s'\n"
            "- or replace it with a plain zone/state alias",
            world_state_name(state),
            world_state_name(state));
    }

    for (size_t input_i = 0;
         input_i < state->data.world_state.input_count;
         input_i++) {
        const char *input_name =
            state->data.world_state.input_names[input_i];
        if (input_name == NULL)
            continue;
        if (state->data.world_state.state_name != NULL
            && strcmp(state->data.world_state.state_name, input_name) == 0) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_WORLD_CONTRACT_INVALID,
                PGY_CAUSE_WORLD_CONTRACT,
                PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION,
                state,
                "Composed world state '%s' cannot reference itself.\n"
                "Reason:\n"
                "- world-state composition must depend on earlier zone/state inputs\n"
                "- self-reference would make the contract cyclic and under-specified\n"
                "Fix:\n"
                "- remove '%s' from its own input list\n"
                "- or split the state into earlier intermediate aliases",
                input_name,
                input_name);
            continue;
        }
        if (find_world_zone_slot_local(world, input_name) != NULL) {
            saw_direct_zone_input = true;
            continue;
        }
        if (find_world_state_before_local(world, input_name, state_index)
            != NULL) {
            saw_state_input = true;
            continue;
        }
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_WORLD_CONTRACT_INVALID,
            PGY_CAUSE_WORLD_CONTRACT,
            PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION,
            state,
            "Composed world state '%s' references unknown or later world zone/state '%s'.\n"
            "Reason:\n"
            "- composed world state inputs must name either a declared zone slot or an earlier world-state alias\n"
            "- '%s' is not visible as a prior input at this point\n"
            "Fix:\n"
            "- reference a declared world zone slot\n"
            "- or move the producer state before '%s'\n"
            "- or correct the input name",
            world_state_name(state),
            input_name,
            input_name,
            world_state_name(state));
    }

    for (size_t input_i = 0;
         input_i < state->data.world_state.input_count;
         input_i++) {
        const char *input_name =
            state->data.world_state.input_names[input_i];
        const char *input_plain_zone;
        if (input_name == NULL)
            continue;
        input_plain_zone = resolve_world_plain_zone_input_name(world,
                                                               input_name);
        for (size_t prev_i = 0; prev_i < input_i; prev_i++) {
            const char *prev_name =
                state->data.world_state.input_names[prev_i];
            const char *prev_plain_zone;
            if (prev_name == NULL)
                continue;
            if (strcmp(prev_name, input_name) == 0) {
                semantic_warning(ctx, state,
                    "Composed world state '%s' repeats input '%s'.\n"
                    "Reason:\n"
                    "- the same upstream zone/state input appears more than once\n"
                    "- duplicate inputs do not add meaning and make provenance noisier\n"
                    "Fix:\n"
                    "- keep a single '%s' input in world state '%s'\n"
                    "- or collapse duplicate entries before composition",
                    world_state_name(state),
                    input_name,
                    input_name,
                    world_state_name(state));
                break;
            }
            prev_plain_zone = resolve_world_plain_zone_input_name(world,
                                                                  prev_name);
            if (input_plain_zone != NULL && prev_plain_zone != NULL
                && strcmp(input_plain_zone, prev_plain_zone) == 0) {
                semantic_warning(ctx, state,
                    "Composed world state '%s' redundantly mixes zone slot '%s' with plain alias/input '%s'.\n"
                    "Reason:\n"
                    "- both inputs resolve to the same underlying zone source\n"
                    "- mixing the raw zone slot and its plain alias duplicates provenance without changing semantics\n"
                    "Fix:\n"
                    "- keep either zone slot '%s' or alias/input '%s'\n"
                    "- prefer one canonical upstream name per composed world state",
                    world_state_name(state),
                    prev_name,
                    input_name,
                    prev_name,
                    input_name);
                break;
            }
        }
        if (find_world_zone_slot_local(world, input_name) != NULL) {
            semantic_warning(ctx, state,
                "Composed world state '%s' directly references zone slot '%s'; prefer a plain 'state name: zone %s' alias so command and derived layers stay separate.\n"
                "Reason:\n"
                "- raw zone slots blur command-layer input with derived world-state composition\n"
                "- a plain alias keeps provenance and layering clearer\n"
                "Fix:\n"
                "- introduce 'state name: zone %s'\n"
                "- and compose from that alias instead of the raw zone slot",
                world_state_name(state),
                input_name,
                input_name,
                input_name);
        }
    }

    if (saw_direct_zone_input && saw_state_input) {
        semantic_warning(ctx, state,
            "Composed world state '%s' mixes direct zone-slot inputs with world-state inputs; prefer composing from world-state aliases only.\n"
            "Reason:\n"
            "- mixing raw zone slots with world-state aliases makes composition provenance harder to read\n"
            "- world-state aliases are the preferred boundary between command and derived layers\n"
            "Fix:\n"
            "- compose from world-state aliases only\n"
            "- or keep the composition entirely in raw zone-slot space, but not both",
            world_state_name(state));
    }
}

static void
check_world_state_detail(ASTNode *state,
                         ASTNode *zone_decl,
                         SemanticContext *ctx,
                         const char *zone_slot_name)
{
    const char *detail_name;
    ASTNode *detail_decl = NULL;

    if (zone_decl == NULL || state->data.world_state.detail_name == NULL)
        return;

    detail_name = state->data.world_state.detail_name;
    if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_PROJECTION) {
        detail_decl = find_zone_domain_slot(zone_decl, detail_name);
        if (detail_decl == NULL || ast_domain_slot_is_subject(detail_decl)) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_WORLD_CONTRACT_INVALID,
                PGY_CAUSE_WORLD_CONTRACT,
                PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION,
                state,
                "World state '%s' references unknown object/tobject projection slot '%s' in zone '%s'.\n"
                "Reason:\n"
                "- world state projection visibility can only observe object/tobject projection slots\n"
                "- zone '%s' has no matching projection slot '%s'\n"
                "Fix:\n"
                "- reference a declared object/tobject projection slot from zone '%s'\n"
                "- or declare projection slot '%s' on that zone",
                world_state_name(state),
                detail_name,
                zone_slot_name,
                zone_slot_name,
                detail_name,
                zone_slot_name,
                detail_name);
        }
    } else if (state->data.world_state.source_kind
               == WORLD_STATE_SOURCE_LAYER) {
        detail_decl = find_zone_layer_slot_local(zone_decl, detail_name);
        if (detail_decl == NULL) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_WORLD_CONTRACT_INVALID,
                PGY_CAUSE_WORLD_CONTRACT,
                PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION,
                state,
                "World state '%s' references unknown layer slot '%s' in zone '%s'.\n"
                "Reason:\n"
                "- world layer visibility expects a declared zone layer slot\n"
                "- zone '%s' has no layer slot named '%s'\n"
                "Fix:\n"
                "- reference a declared layer slot from zone '%s'\n"
                "- or declare layer slot '%s' on that zone",
                world_state_name(state),
                detail_name,
                zone_slot_name,
                zone_slot_name,
                detail_name,
                zone_slot_name,
                detail_name);
        }
    } else if (state->data.world_state.source_kind
               == WORLD_STATE_SOURCE_STATE) {
        detail_decl = find_zone_state_decl_local(zone_decl, detail_name);
        if (detail_decl == NULL) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_WORLD_CONTRACT_INVALID,
                PGY_CAUSE_WORLD_CONTRACT,
                PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION,
                state,
                "World state '%s' references unknown zone state '%s' in zone '%s'.\n"
                "Reason:\n"
                "- world state composition expects a declared zone state alias\n"
                "- zone '%s' has no state named '%s'\n"
                "Fix:\n"
                "- reference a declared zone state from zone '%s'\n"
                "- or declare zone state '%s' before composing it into the world",
                world_state_name(state),
                detail_name,
                zone_slot_name,
                zone_slot_name,
                detail_name,
                zone_slot_name,
                detail_name);
        }
    }
}

static void
check_world_state_duplicate(ASTNode *world,
                            ASTNode *state,
                            SemanticContext *ctx,
                            size_t state_index)
{
    size_t state_count = 0;
    ASTNode **states = ast_world_states(world, &state_count);

    for (size_t j = state_index + 1; j < state_count; j++) {
        ASTNode *other = states[j];
        if (other != NULL
            && other->data.world_state.state_name != NULL
            && state->data.world_state.state_name != NULL
            && strcmp(other->data.world_state.state_name,
                      state->data.world_state.state_name) == 0) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_REDECLARATION,
                PGY_CAUSE_WORLD_STATE_DUPLICATE_NAME,
                PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
                other,
                "Redeclaration of world state '%s'",
                state->data.world_state.state_name);
        }
    }
}

void
type_check_world_states(ASTNode *world, SemanticContext *ctx)
{
    size_t state_count = 0;
    ASTNode **states = ast_world_states(world, &state_count);

    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        const char *zone_slot_name = state->data.world_state.zone_slot_name;
        ASTNode *zone_decl = NULL;

        if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
            || state->data.world_state.source_kind == WORLD_STATE_SOURCE_ANY) {
            check_composed_world_state(world, state, ctx, i);
        } else if (find_world_zone_slot_local(world, zone_slot_name) == NULL) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_WORLD_CONTRACT_INVALID,
                PGY_CAUSE_WORLD_CONTRACT,
                PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION,
                state,
                "World state '%s' references unknown zone slot '%s'",
                world_state_name(state),
                zone_slot_name != NULL ? zone_slot_name : "<unknown>");
        } else {
            zone_decl = resolve_world_zone_decl_local(world, ctx,
                                                     zone_slot_name);
        }

        check_world_state_detail(state, zone_decl, ctx, zone_slot_name);
        check_world_state_duplicate(world, state, ctx, i);
    }
}

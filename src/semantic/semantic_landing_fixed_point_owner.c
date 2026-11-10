#include "semantic_landing_fixed_point_owner.h"

#include "../common/string_compat.h"
#include "../parser/ast_api.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t *ids;
    size_t count;
} SemanticLandingPlan;

typedef struct {
    uint32_t *ids;
    PgyStringStoreLandingKind *kinds;
    size_t count;
} SemanticStoreLandingPlan;

static void
semantic_landing_error(char **error_message, const char *message)
{
    if (error_message != NULL && *error_message == NULL)
        *error_message = pergyra_strdup(message);
}

/* Landing identities come only from the unchanged source namespace.  Set
 * growth is therefore the fixed point's termination measure. */
static bool
semantic_landing_plan_merge(SemanticLandingPlan *plan,
                            const uint32_t *ids,
                            size_t count,
                            uint32_t stable_id_ceiling,
                            bool *grew)
{
    if (plan == NULL || grew == NULL || (count != 0 && ids == NULL))
        return false;
    for (size_t i = 0; i < count; i++) {
        if (ids[i] == 0 || ids[i] >= stable_id_ceiling)
            return false;
        bool known = false;
        for (size_t j = 0; j < plan->count && !known; j++)
            known = plan->ids[j] == ids[i];
        if (known)
            continue;
        if (plan->count == SIZE_MAX ||
            plan->count + 1 > SIZE_MAX / sizeof(*plan->ids))
            return false;
        uint32_t *grown = realloc(
            plan->ids, (plan->count + 1) * sizeof(*plan->ids));
        if (grown == NULL)
            return false;
        plan->ids = grown;
        plan->ids[plan->count++] = ids[i];
        *grew = true;
    }
    return true;
}

static bool
semantic_store_landing_plan_merge(SemanticStoreLandingPlan *plan,
                                  const uint32_t *ids,
                                  const PgyStringStoreLandingKind *kinds,
                                  size_t count,
                                  uint32_t stable_id_ceiling,
                                  bool *grew)
{
    if (plan == NULL || grew == NULL ||
        (count != 0 && (ids == NULL || kinds == NULL)))
        return false;
    for (size_t i = 0; i < count; i++) {
        if (ids[i] == 0 || ids[i] >= stable_id_ceiling ||
            (kinds[i] != PGY_STRING_STORE_LANDING_VALUE_COPY &&
             kinds[i] != PGY_STRING_STORE_LANDING_OWNED_PUSH &&
             kinds[i] != PGY_STRING_STORE_LANDING_OWNED_SET &&
             kinds[i] != PGY_STRING_STORE_LANDING_MEMBER_VALUE_COPY))
            return false;
        bool known = false;
        for (size_t j = 0; j < plan->count; j++) {
            if (plan->ids[j] != ids[i])
                continue;
            /* Store plans refine monotonically. An owned replacement copies
             * the new String before freeing the old element, so it includes
             * the earlier value-only copy obligation. The member-copy kind is
             * a fallback proof for that same transition; a later formal
             * owned-push proof supersedes it, never the reverse. */
            if (plan->kinds[j] == PGY_STRING_STORE_LANDING_VALUE_COPY &&
                (kinds[i] == PGY_STRING_STORE_LANDING_OWNED_PUSH ||
                 kinds[i] == PGY_STRING_STORE_LANDING_OWNED_SET ||
                 kinds[i] == PGY_STRING_STORE_LANDING_MEMBER_VALUE_COPY)) {
                plan->kinds[j] = kinds[i];
                *grew = true;
                known = true;
                break;
            }
            if (plan->kinds[j]
                    == PGY_STRING_STORE_LANDING_MEMBER_VALUE_COPY
                && kinds[i] == PGY_STRING_STORE_LANDING_OWNED_PUSH) {
                plan->kinds[j] = kinds[i];
                *grew = true;
                known = true;
                break;
            }
            if (plan->kinds[j] == PGY_STRING_STORE_LANDING_OWNED_PUSH
                && kinds[i]
                    == PGY_STRING_STORE_LANDING_MEMBER_VALUE_COPY) {
                known = true;
                break;
            }
            if (plan->kinds[j] != kinds[i])
                return false;
            known = true;
            break;
        }
        if (known)
            continue;
        if (plan->count == SIZE_MAX ||
            plan->count + 1 > SIZE_MAX / sizeof(*plan->ids) ||
            plan->count + 1 > SIZE_MAX / sizeof(*plan->kinds))
            return false;
        uint32_t *grown_ids = malloc(
            (plan->count + 1) * sizeof(*grown_ids));
        PgyStringStoreLandingKind *grown_kinds = malloc(
            (plan->count + 1) * sizeof(*grown_kinds));
        if (grown_ids == NULL || grown_kinds == NULL) {
            free(grown_ids);
            free(grown_kinds);
            return false;
        }
        if (plan->count != 0) {
            memcpy(grown_ids, plan->ids,
                plan->count * sizeof(*grown_ids));
            memcpy(grown_kinds, plan->kinds,
                plan->count * sizeof(*grown_kinds));
        }
        free(plan->ids);
        free(plan->kinds);
        plan->ids = grown_ids;
        plan->kinds = grown_kinds;
        plan->ids[plan->count] = ids[i];
        plan->kinds[plan->count] = kinds[i];
        plan->count++;
        *grew = true;
    }
    return true;
}

SemanticResult *
semantic_analyze_landing_fixed_point(ASTNode **ast,
                                     SemanticLandingReload reload,
                                     void *reload_context,
                                     char **error_message)
{
    if (ast == NULL || *ast == NULL || reload == NULL) {
        semantic_landing_error(error_message,
            "semantic landing requires one reloadable source program");
        return NULL;
    }
    uint32_t stable_id_ceiling = ast_stable_id_ceiling(*ast);
    if (stable_id_ceiling == 0) {
        semantic_landing_error(error_message,
            "semantic landing source identity namespace is invalid");
        return NULL;
    }

    SemanticStoreLandingPlan stores = {0};
    SemanticLandingPlan copies = {0};
    SemanticResult *semantic = semantic_analyze(*ast);
    size_t round = 0;
    while (semantic != NULL) {
        bool round_trace = getenv("PGY_DEBUG_LANDING_ROUNDS") != NULL;
        size_t stores_before = stores.count;
        size_t copies_before = copies.count;
        bool stores_grew = false;
        bool copies_grew = false;
        if (round_trace || getenv("PGY_DEBUG_PIPELINE_STAGE") != NULL)
            fprintf(stderr,
                "[semantic landing] round=%zu selected_stores=%zu selected_copies=%zu cumulative_stores=%zu cumulative_copies=%zu\n",
                round, semantic->string_store_landing_count,
                semantic->loan_copy_landing_count,
                stores.count, copies.count);
        if (!semantic_store_landing_plan_merge(&stores,
                semantic->string_store_landings,
                semantic->string_store_landing_kinds,
                semantic->string_store_landing_count,
                stable_id_ceiling, &stores_grew)) {
            semantic_landing_error(error_message,
                "semantic landing selected an identity it cannot land");
            semantic_result_destroy(semantic);
            semantic = NULL;
            break;
        }
        /* Store evidence owns the outer fixed point. A newly admitted store
         * can make an earlier loan copy unnecessary, so do not retain the
         * copy plan across store growth. The unchanged source must select
         * again every copy that remains necessary with the enlarged store
         * plan; a missing receipt still fails closed on that replay. */
        if (stores_grew) {
            free(copies.ids);
            copies = (SemanticLandingPlan){0};
        } else if (!semantic_landing_plan_merge(&copies,
                       semantic->loan_copy_landings,
                       semantic->loan_copy_landing_count,
                       stable_id_ceiling, &copies_grew)) {
            semantic_landing_error(error_message,
                "semantic landing selected an identity it cannot land");
            semantic_result_destroy(semantic);
            semantic = NULL;
            break;
        }
        if (round_trace)
            fprintf(stderr,
                "[semantic landing result] round=%zu new_stores=%zu selected_copies=%zu cumulative_stores=%zu cumulative_copies=%zu reset_copies=%zu grew=%d\n",
                round, stores.count - stores_before,
                semantic->loan_copy_landing_count,
                stores.count, copies.count,
                stores_grew ? copies_before : 0,
                (stores_grew || copies_grew) ? 1 : 0);
        if (!stores_grew && !copies_grew)
            break;

        round++;
        semantic_result_destroy(semantic);
        semantic = NULL;
        ast_destroy(*ast);
        *ast = reload(reload_context, error_message);
        if (*ast == NULL)
            break;
        if (ast_stable_id_ceiling(*ast) != stable_id_ceiling) {
            semantic_landing_error(error_message,
                "semantic landing source identity changed after reload");
            break;
        }
        semantic = semantic_analyze_with_landings(
            *ast, stores.ids, stores.kinds, stores.count,
            copies.ids, copies.count);
    }
    free(stores.ids);
    free(stores.kinds);
    free(copies.ids);
    return semantic;
}

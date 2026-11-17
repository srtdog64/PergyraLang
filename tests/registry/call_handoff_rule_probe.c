/* Decision cells of the call hand-off rule as the native front end reads
 * them (src/semantic/call_handoff_rule.h and .c, rendered from
 * src/semantic/call_handoff_rule.def). Named cells are checked here; with
 * --dump the probe prints every cell, which call_handoff_rule_smoke.sh
 * compares with the self-host rendering (call_handoff_rule_probe.pgy). */
#include <stdio.h>
#include <string.h>

#include "semantic/call_handoff_rule.h"

static int failures;

static void
expect(int line, int got, int want)
{
    if (got == want) return;
    fprintf(stderr, "call_handoff_rule_probe.c:%d: got %d, want %d\n", line, got, want);
    failures++;
}

#define EXPECT(got, want) expect(__LINE__, (int)(got), (int)(want))
#define K(name) PGY_CALL_HANDOFF_KIND_##name
#define O(name) PGY_CALL_HANDOFF_ORIGIN_##name
#define L(name) PGY_CALL_HANDOFF_LATER_##name
#define C(name) PGY_CALL_HANDOFF_CONTEXT_##name
#define E(name) PGY_CALL_HANDOFF_ELEMENTS_##name
#define A(name) PGY_CALL_HANDOFF_ACTION_##name
#define M(name) PGY_CALL_HANDOFF_MEMBER_##name
#define D(name) PGY_CALL_HANDOFF_DIVERGENCE_##name

static int
act(PgyCallHandoffKind kind, PgyCallHandoffOrigin origin, PgyCallHandoffLater later,
    PgyCallHandoffContext context, PgyCallHandoffElements elements, unsigned divergences)
{
    return (int)pgy_call_handoff_action_for_kind(kind, origin, later, context,
        elements, divergences);
}

static void
check_join(void)
{
    EXPECT(pgy_call_handoff_kind(0u), K(READ_ONLY));
    EXPECT(pgy_call_handoff_kind(M(READ)), K(READ_ONLY));
    EXPECT(pgy_call_handoff_kind(M(RETURN)), K(RETURN_ONLY));
    EXPECT(pgy_call_handoff_kind(M(READ) | M(RETURN)), K(RETURN_ONLY));
    EXPECT(pgy_call_handoff_kind(M(KEEP)), K(KEEP_ONLY));
    EXPECT(pgy_call_handoff_kind(M(READ) | M(KEEP)), K(KEEP_ONLY));
    EXPECT(pgy_call_handoff_kind(M(RETURN) | M(KEEP)), K(MOVING));
    EXPECT(pgy_call_handoff_kind(M(GROW)), K(MOVING));
    EXPECT(pgy_call_handoff_kind(M(RELEASE)), K(MOVING));
    EXPECT(pgy_call_handoff_kind(M(UNPROVED)), K(UNPROVED));
    EXPECT(pgy_call_handoff_kind(M(GROW) | M(UNPROVED)), K(UNPROVED));
    EXPECT(pgy_call_handoff_kind(1u << 6), K(UNPROVED));
    EXPECT(pgy_call_handoff_members_from_native_summary(SLOT_PARAM_SUMMARY_READ), M(READ));
    EXPECT(pgy_call_handoff_members_from_native_summary(SLOT_PARAM_SUMMARY_WRITE), M(GROW));
    EXPECT(pgy_call_handoff_members_from_native_summary(SLOT_PARAM_SUMMARY_RELEASE), M(RELEASE));
    EXPECT(pgy_call_handoff_members_from_native_summary(SLOT_PARAM_SUMMARY_RETURN_ESCAPE), M(RETURN));
    EXPECT(pgy_call_handoff_members_from_native_summary(SLOT_PARAM_SUMMARY_CALL_ESCAPE), M(KEEP));
    EXPECT(pgy_call_handoff_members_from_native_summary(SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE), M(KEEP));
    EXPECT(pgy_call_handoff_members_from_native_summary(1u << 6), M(UNPROVED));
}

/* Study 2.2 and 5.3, the user decision on unproved effects, and the
 * element ownership axis. */
static void
check_rule(void)
{
    for (int o = 0; o < PGY_CALL_HANDOFF_ORIGIN_COUNT; o++)
        for (int l = 0; l < PGY_CALL_HANDOFF_LATER_COUNT; l++)
            for (int c = 0; c < PGY_CALL_HANDOFF_CONTEXT_COUNT; c++) {
                EXPECT(act(K(READ_ONLY), o, l, c, E(AS_GIVEN), 0u), A(LEND_FOR_CALL));
                EXPECT(act(K(UNPROVED), o, l, C(DETACHED), E(AS_GIVEN), 0u), A(REFUSE));
                EXPECT(act(K(KEEP_ONLY), o, l, c, E(OWNED_REQUIRED), 0u), A(OWNED_ELEMENT_CLONE));
            }
    EXPECT(act(K(RETURN_ONLY), O(FRESH), L(ONCE), C(SYNC), E(AS_GIVEN), 0u), A(RESULT_OWNS));
    EXPECT(act(K(RETURN_ONLY), O(LOCAL), L(ONCE), C(SYNC), E(AS_GIVEN), 0u), A(HAND_OFF));
    EXPECT(act(K(RETURN_ONLY), O(CALLER_FORMAL), L(ONCE), C(SYNC), E(AS_GIVEN), 0u), A(MOVED_ON));
    EXPECT(act(K(RETURN_ONLY), O(LOCAL), L(USED_AGAIN), C(SYNC), E(AS_GIVEN), 0u), A(LOAN_TO_RESULT));
    EXPECT(act(K(RETURN_ONLY), O(CALLER_FORMAL), L(USED_AGAIN), C(SYNC), E(AS_GIVEN), 0u), A(LOAN_TO_RESULT));
    EXPECT(act(K(RETURN_ONLY), O(BORROWED), L(ONCE), C(SYNC), E(AS_GIVEN), 0u), A(LOAN_TO_RESULT));
    EXPECT(act(K(MOVING), O(FRESH), L(ONCE), C(SYNC), E(AS_GIVEN), 0u), A(MOVED));
    EXPECT(act(K(MOVING), O(LOCAL), L(ONCE), C(SYNC), E(AS_GIVEN), 0u), A(HAND_OFF));
    EXPECT(act(K(MOVING), O(LOCAL), L(USED_AGAIN), C(SYNC), E(AS_GIVEN), 0u), A(CLONE_ARGUMENT));
    EXPECT(act(K(MOVING), O(LOCAL), L(ONCE), C(DETACHED), E(AS_GIVEN), 0u), A(CLONE_ARGUMENT));
    EXPECT(act(K(MOVING), O(CALLER_FORMAL), L(ONCE), C(SYNC), E(AS_GIVEN), 0u), A(MOVED_ON));
    EXPECT(act(K(MOVING), O(CALLER_FORMAL), L(USED_AGAIN), C(SYNC), E(AS_GIVEN), 0u), A(CLONE_ARGUMENT));
    EXPECT(act(K(MOVING), O(BORROWED), L(ONCE), C(SYNC), E(AS_GIVEN), 0u), A(CLONE_ARGUMENT));
    EXPECT(act(K(KEEP_ONLY), O(LOCAL), L(ONCE), C(SYNC), E(AS_GIVEN), 0u), A(HAND_OFF));
    EXPECT(act(K(KEEP_ONLY), O(LOCAL), L(USED_AGAIN), C(SYNC), E(AS_GIVEN), 0u), A(CLONE_ARGUMENT));
    EXPECT(act(K(UNPROVED), O(LOCAL), L(ONCE), C(SYNC), E(AS_GIVEN), 0u), A(HAND_OFF));
    EXPECT(act(K(UNPROVED), O(LOCAL), L(USED_AGAIN), C(SYNC), E(AS_GIVEN), 0u), A(CLONE_ARGUMENT));
    EXPECT(pgy_call_handoff_action(M(GROW), O(LOCAL), L(ONCE), C(SYNC), E(AS_GIVEN), 0u), A(HAND_OFF));
    EXPECT(pgy_call_handoff_action_any_use(K(READ_ONLY), C(SYNC), E(AS_GIVEN)), A(LEND_FOR_CALL));
    EXPECT(pgy_call_handoff_action_any_use(K(KEEP_ONLY), C(SYNC), E(OWNED_REQUIRED)), A(OWNED_ELEMENT_CLONE));
    EXPECT(pgy_call_handoff_action_any_use(K(UNPROVED), C(DETACHED), E(AS_GIVEN)), A(REFUSE));
    EXPECT(pgy_call_handoff_action_any_use(K(MOVING), C(SYNC), E(AS_GIVEN)), A(UNDECIDED));
    EXPECT(act(K(COUNT), O(LOCAL), L(ONCE), C(SYNC), E(AS_GIVEN), 0u), A(UNDECIDED));
}

/* The divergence rows give today's behaviour only when selected. */
static void
check_divergences(void)
{
    const unsigned native = PGY_CALL_HANDOFF_NATIVE_ROW_DIVERGENCES;
    EXPECT(act(K(KEEP_ONLY), O(LOCAL), L(USED_AGAIN), C(SYNC), E(AS_GIVEN), native), A(REFUSE));
    EXPECT(act(K(KEEP_ONLY), O(BORROWED), L(USED_AGAIN), C(SYNC), E(AS_GIVEN), native), A(REFUSE));
    EXPECT(act(K(UNPROVED), O(LOCAL), L(USED_AGAIN), C(SYNC), E(AS_GIVEN), native), A(REFUSE));
    EXPECT(act(K(MOVING), O(LOCAL), L(USED_AGAIN), C(SYNC), E(AS_GIVEN), native), A(CLONE_ARGUMENT));
    EXPECT(act(K(RETURN_ONLY), O(LOCAL), L(USED_AGAIN), C(SYNC), E(AS_GIVEN), native), A(LOAN_TO_RESULT));
    EXPECT(act(K(KEEP_ONLY), O(LOCAL), L(ONCE), C(SYNC), E(AS_GIVEN), native), A(HAND_OFF));
    EXPECT(act(K(KEEP_ONLY), O(LOCAL), L(ONCE), C(SYNC), E(AS_GIVEN),
        D(SELF_RETIRED_CONSTRUCTOR_INPUT_KEPT)), A(KEEP_ROW_STATE));
    EXPECT(act(K(KEEP_ONLY), O(FRESH), L(ONCE), C(SYNC), E(AS_GIVEN),
        D(SELF_RETIRED_CONSTRUCTOR_INPUT_KEPT)), A(MOVED));
    EXPECT(act(K(KEEP_ONLY), O(LOCAL), L(USED_AGAIN), C(SYNC), E(AS_GIVEN),
        D(SELF_RETIRED_CONSTRUCTOR_INPUT_KEPT)), A(CLONE_ARGUMENT));
}

static void
dump(void)
{
    for (int d = -1; d < (int)PGY_CALL_HANDOFF_DIVERGENCE_FLAG_COUNT; d++) {
        unsigned divergences = d < 0 ? 0u : 1u << d;
        for (int k = 0; k < PGY_CALL_HANDOFF_KIND_COUNT; k++)
            for (int o = 0; o < PGY_CALL_HANDOFF_ORIGIN_COUNT; o++)
                for (int l = 0; l < PGY_CALL_HANDOFF_LATER_COUNT; l++)
                    for (int c = 0; c < PGY_CALL_HANDOFF_CONTEXT_COUNT; c++)
                        for (int e = 0; e < PGY_CALL_HANDOFF_ELEMENTS_COUNT; e++)
                            printf("cell %u %d %d %d %d %d %d\n", divergences, k, o, l, c, e,
                                act(k, o, l, c, e, divergences));
    }
    for (unsigned m = 0; m <= PGY_CALL_HANDOFF_MEMBER_ALL; m++)
        printf("kind %u %d\n", m, (int)pgy_call_handoff_kind(m));
    /* The MIR JSON spellings the native writer uses (slice S2). */
    for (unsigned i = 0; pgy_call_handoff_member_name(i) != NULL; i++)
        printf("name member %u %s\n", i, pgy_call_handoff_member_name(i));
    for (int i = 0; i < PGY_CALL_HANDOFF_ORIGIN_COUNT; i++)
        printf("name origin %d %s\n", i, pgy_call_handoff_origin_name((PgyCallHandoffOrigin)i));
    for (int i = 0; i < PGY_CALL_HANDOFF_LATER_COUNT; i++)
        printf("name later %d %s\n", i, pgy_call_handoff_later_name((PgyCallHandoffLater)i));
    for (int i = 0; i < PGY_CALL_HANDOFF_CONTEXT_COUNT; i++)
        printf("name context %d %s\n", i, pgy_call_handoff_context_name((PgyCallHandoffContext)i));
    for (int i = 0; i < PGY_CALL_HANDOFF_ELEMENTS_COUNT; i++)
        printf("name elements %d %s\n", i, pgy_call_handoff_elements_name((PgyCallHandoffElements)i));
    for (int i = 0; i < PGY_CALL_HANDOFF_ACTION_UNDECIDED; i++)
        printf("name action %d %s\n", i, pgy_call_handoff_action_name((PgyCallHandoffAction)i));
    for (int i = 0; i < PGY_CALL_HANDOFF_DIVERGENCE_ID_COUNT; i++)
        printf("name divergence %d %s\n", i,
            pgy_call_handoff_divergence_name((PgyCallHandoffDivergenceId)i));
    const char *producers[] = {"Concat", "Substring", "ToString", "Clone", "UserConcat"};
    const char *types[] = {"String", "Int", "Long", "Bool", "Unknown"};
    for (int producer = 0; producer < 5; producer++)
        for (int arity = 1; arity <= 4; arity++)
            for (int type = 0; type < 5; type++)
                printf("result %d %d %d %d\n", producer, arity, type,
                    pgy_call_handoff_string_result_fresh(producers[producer], arity, types[type]));
}

int
main(int argc, char **argv)
{
    check_join();
    check_rule();
    check_divergences();
    EXPECT(pgy_call_handoff_string_result_fresh("Concat", 2, "String"), true);
    EXPECT(pgy_call_handoff_string_result_fresh("ToString", 1, "Int"), true);
    EXPECT(pgy_call_handoff_string_result_fresh("ToString", 1, "String"), false);
    EXPECT(pgy_call_handoff_string_result_fresh("Clone", 1, "Array<String>"), false);
    EXPECT(pgy_call_handoff_string_result_fresh(NULL, 1, "Int"), false);
    EXPECT(pgy_call_handoff_string_result_fresh("Concat", 2, NULL), false);
    if (failures != 0) return 1;
    if (argc == 2 && strcmp(argv[1], "--dump") == 0) dump();
    return 0;
}

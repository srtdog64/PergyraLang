#include "../src/common/arena.h"

#include <stdio.h>
#include <string.h>

int
main(void)
{
    PgyArena arena;
    const PgyArenaLedger *ledger;

    pgy_arena_init_named(&arena, 16, "arena-ledger-fixture");
    if (pgy_arena_strdup(&arena, "hello") == NULL
        || pgy_arena_alloc(&arena, 24) == NULL)
        return 1;
    pgy_arena_set_last_consumer(&arena, "fixture-consumer");
    pgy_arena_set_release_point(&arena, "fixture-release");
    pgy_arena_note_cross_stage_copy(&arena, 24);
    pgy_arena_note_identity_copy(&arena, sizeof(unsigned long));

    ledger = pgy_arena_ledger(&arena);
    if (ledger == NULL || strcmp(ledger->owner, "arena-ledger-fixture") != 0
        || strcmp(ledger->last_consumer, "fixture-consumer") != 0
        || strcmp(ledger->release_point, "fixture-release") != 0
        || ledger->created_bytes < 32
        || ledger->retained_bytes < 32
        || ledger->peak_bytes != ledger->retained_bytes
        || ledger->allocation_count != 2
        || ledger->string_payload_bytes != 6
        || ledger->cross_stage_copies != 24
        || ledger->identity_payload_bytes != sizeof(unsigned long))
        return 2;

    pgy_arena_destroy(&arena);
    puts("[arena-ledger] PASS owner, lifetime, copy, and peak fields");
    return 0;
}

#ifndef PGY_ZONE_SYNC_GENERATED_C
#error "PGY_ZONE_SYNC_GENERATED_C must name the generated C fixture"
#endif

#define main pgy_generated_main
#include PGY_ZONE_SYNC_GENERATED_C
#undef main

int main(void)
{
    EmptyZone zone = {0};
    int result = 0;

    /* This explicit lifecycle is ABI-only supporting evidence. Generated
     * fresh-local lifecycle is exercised by the source fixture separately. */
    PGY_ZONE_LOCK_INIT(&zone);

    zone.__projection_ready_view = true;
    zone.__projection_dirty_view = false;
    zone.__projection_epoch_view = 7u;
    zone.__projection_cause_view = 11;
    zone.__projection_ready_packet = false;
    zone.__projection_dirty_packet = true;
    zone.__projection_epoch_packet = 13u;
    zone.__projection_cause_packet = 17;

    EmptyZone_sync(&zone);
    EmptyZone_sync(&zone);

    if (PGY_ZONE_GENERATION_LOAD(&zone) != 2u)
        result = 1;
    else if (!zone.__projection_ready_view || zone.__projection_dirty_view ||
             zone.__projection_epoch_view != 7u ||
             zone.__projection_cause_view != 11)
        result = 2;
    else if (zone.__projection_ready_packet ||
             !zone.__projection_dirty_packet ||
             zone.__projection_epoch_packet != 13u ||
             zone.__projection_cause_packet != 17)
        result = 3;

    PGY_ZONE_LOCK_DESTROY(&zone);
    return result;
}

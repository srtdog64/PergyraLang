/*
 * Copyright (c) 2025 Pergyra Language Project
 * World-Roster Runtime Lookup Helpers
 */

#include "world_roster.h"

#include <string.h>

PartyContext*
RosterFindParty(RosterContext* roster, const char* partySlot)
{
    if (roster == NULL || partySlot == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < roster->partyCount; i++) {
        if (roster->partySlots[i].slotName != NULL
            && strcmp(roster->partySlots[i].slotName, partySlot) == 0) {
            return roster->partySlots[i].partyContext;
        }
    }
    return NULL;
}

RosterContext*
WorldFindRoster(WorldContext* world, const char* rosterSlot)
{
    if (world == NULL || rosterSlot == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < world->rosterCount; i++) {
        if (world->rosters[i].slotName != NULL
            && strcmp(world->rosters[i].slotName, rosterSlot) == 0) {
            return world->rosters[i].instance;
        }
    }
    return NULL;
}

PartyContext*
WorldFindParty(WorldContext* world, const char* rosterSlot, const char* partySlot)
{
    RosterContext* roster = WorldFindRoster(world, rosterSlot);
    return RosterFindParty(roster, partySlot);
}

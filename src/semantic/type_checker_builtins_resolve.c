#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "builtin_kind.h"
#include "../common/intent_observability_abi.h"

typedef struct BuiltinEntry {
    const char *name;
    BuiltinKind kind;
} BuiltinEntry;

static const BuiltinEntry k_builtin_entries[] = {
    {"AllocatorDebug", BUILTIN_ALLOCATOR_DEBUG},
    {"AllocatorDestroy", BUILTIN_ALLOCATOR_DESTROY},
    {"AllocatorPersistent", BUILTIN_ALLOCATOR_PERSISTENT},
    {"AllocatorPool", BUILTIN_ALLOCATOR_POOL},
    {"AllocatorResult", BUILTIN_ALLOCATOR_RESULT},
    {"AllocatorScratch", BUILTIN_ALLOCATOR_SCRATCH},
    {"AllocatorSystem", BUILTIN_ALLOCATOR_SYSTEM},
    {"AllocatorTracing", BUILTIN_ALLOCATOR_TRACING},
    {"Args", BUILTIN_ARGS},
    {"Box", BUILTIN_BOX},
    {"BoxArray", BUILTIN_BOX_ARRAY},
    {"BoxDrop", BUILTIN_BOX_DROP},
    {"BoxGet", BUILTIN_BOX_GET},
    {"BoxIsValid", BUILTIN_BOX_IS_VALID},
    {"BoxSet", BUILTIN_BOX_SET},
    {"ClaimDeviceSlot", BUILTIN_CLAIM_DEVICE_SLOT},
    {"ClaimQubit", BUILTIN_NOT_BUILTIN},
    {"ClaimSecureSlot", BUILTIN_CLAIM_SECURE_SLOT},
    {"ClaimSlot", BUILTIN_CLAIM_SLOT},
    {"Clone", BUILTIN_CLONE},
    {"CompilerArtifactAbort", BUILTIN_COMPILER_ARTIFACT_ABORT},
    {"CompilerArtifactBegin", BUILTIN_COMPILER_ARTIFACT_BEGIN},
    {"CompilerArtifactCommit", BUILTIN_COMPILER_ARTIFACT_COMMIT},
    {"CompilerArtifactWrite", BUILTIN_COMPILER_ARTIFACT_WRITE},
    {"DeviceRead", BUILTIN_DEVICE_READ},
    {"DeviceWrite", BUILTIN_DEVICE_WRITE},
    {"DirWalk", BUILTIN_DIR_WALK},
    {"Entangle", BUILTIN_NOT_BUILTIN},
    {"Exit", BUILTIN_EXIT},
    {"FileClose", BUILTIN_FILE_CLOSE},
    {"FileExists", BUILTIN_FILE_EXISTS},
    {"FileOpen", BUILTIN_FILE_OPEN},
    {"FileRead", BUILTIN_FILE_READ},
    {"FileWrite", BUILTIN_FILE_WRITE},
    {"HasLayer", BUILTIN_HAS_LAYER},
    {"HasProjection", BUILTIN_HAS_PROJECTION},
    {"HasState", BUILTIN_HAS_STATE},
    {"HasZone", BUILTIN_HAS_ZONE},
    {"HasZoneLayer", BUILTIN_HAS_ZONE_LAYER},
    {"HasZoneProjection", BUILTIN_HAS_ZONE_PROJECTION},
    {"HasZoneState", BUILTIN_HAS_ZONE_STATE},
    {"Input", BUILTIN_INPUT},
    {"IsCollapsed", BUILTIN_NOT_BUILTIN},
    {"Log", BUILTIN_LOG},
    {"LogBanner", BUILTIN_LOG_BANNER},
    {"LogBlock", BUILTIN_LOG_BLOCK},
    {"LogRaw", BUILTIN_LOG_RAW},
    {"Measure", BUILTIN_NOT_BUILTIN},
    {"Move", BUILTIN_MOVE},
    {"Now", BUILTIN_NOW},
    {"Print", BUILTIN_PRINT},
    {"QubitState", BUILTIN_NOT_BUILTIN},
    {"RcClone", BUILTIN_RC_CLONE},
    {"RcDowngrade", BUILTIN_RC_DOWNGRADE},
    {"RcDrop", BUILTIN_RC_DROP},
    {"RcGet", BUILTIN_RC_GET},
    {"RcNew", BUILTIN_RC_NEW},
    {"Read", BUILTIN_READ},
    {"ReadFile", BUILTIN_READ_FILE},
    {"ReadLine", BUILTIN_READ_LINE},
    {"ReadStdin", BUILTIN_READ_STDIN},
    {"Release", BUILTIN_RELEASE},
    {"ReleaseDeviceSlot", BUILTIN_RELEASE_DEVICE_SLOT},
    {"ReleaseQubit", BUILTIN_NOT_BUILTIN},
    {"Sleep", BUILTIN_SLEEP},
    {"SlotRawPointer", BUILTIN_SLOT_RAW_POINTER},
    {"StringConcat", BUILTIN_NOT_BUILTIN},
    {"StringContains", BUILTIN_NOT_BUILTIN},
    {"StringIndexOf", BUILTIN_NOT_BUILTIN},
    {"StringJoin", BUILTIN_NOT_BUILTIN},
    {"StringReplace", BUILTIN_NOT_BUILTIN},
    {"StringSplit", BUILTIN_NOT_BUILTIN},
    {"StringTrim", BUILTIN_NOT_BUILTIN},
    {"SubmitDeviceRead", BUILTIN_SUBMIT_DEVICE_READ},
    {"Substring", BUILTIN_NOT_BUILTIN},
    {"TextBuilderAppend", BUILTIN_TEXT_BUILDER_APPEND},
    {"TextBuilderDrop", BUILTIN_TEXT_BUILDER_DROP},
    {"TextBuilderFinish", BUILTIN_TEXT_BUILDER_FINISH},
    {"TextBuilderNew", BUILTIN_TEXT_BUILDER_NEW},
    {"ToLower", BUILTIN_NOT_BUILTIN},
    {"ToObject", BUILTIN_TO_OBJECT},
    {"ToTObject", BUILTIN_TO_TOBJECT},
    {"ToUpper", BUILTIN_NOT_BUILTIN},
    {"ViewRead", BUILTIN_VIEW_READ},
    {"ViewWrite", BUILTIN_VIEW_WRITE},
    {"WeakDrop", BUILTIN_WEAK_DROP},
    {"WeakUpgrade", BUILTIN_WEAK_UPGRADE},
    {"Write", BUILTIN_WRITE},
    {"WriteFile", BUILTIN_WRITE_FILE},
};

static int
compare_builtin_entry(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const BuiltinEntry *candidate = (const BuiltinEntry *)entry;
    return strcmp(name, candidate->name);
}

BuiltinKind
builtin_resolve(const char *name)
{
    if (name == NULL)
        return BUILTIN_NOT_BUILTIN;
    if (pgy_intent_observability_name_is_builtin(name))
        return BUILTIN_INTENT_OBSERVABILITY;

    const BuiltinEntry *entry = bsearch(
        name,
        k_builtin_entries,
        sizeof(k_builtin_entries) / sizeof(k_builtin_entries[0]),
        sizeof(k_builtin_entries[0]),
        compare_builtin_entry);

    return entry != NULL ? entry->kind : BUILTIN_NOT_BUILTIN;
}

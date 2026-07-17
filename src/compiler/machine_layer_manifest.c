#include "machine_layer_manifest.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum
{
    PGY_MACHINE_OP_CLAIM = 1u << 0,
    PGY_MACHINE_OP_READ = 1u << 1,
    PGY_MACHINE_OP_WRITE = 1u << 2,
    PGY_MACHINE_OP_RELEASE = 1u << 3,
    PGY_MACHINE_OP_SUBMIT_READ = 1u << 4
};

static const PgyMachineLayerOperationManifest
PGY_ABSTRACT_DEVICE_SLOT_OPERATIONS[] = {
    { RIR_MACHINE_CONTACT_CLAIM, "claim", "Claim", true, true },
    { RIR_MACHINE_CONTACT_READ, "read", "Read", true, true },
    { RIR_MACHINE_CONTACT_WRITE, "write", "Write", true, true },
    { RIR_MACHINE_CONTACT_RELEASE, "release", "Release", true, true },
    { RIR_MACHINE_CONTACT_SUBMIT_READ, "submit-read", "SubmitRead", true, true },
};

static const PgyMachineLayerProjectionManifest
PGY_ABSTRACT_DEVICE_SLOT_PROJECTIONS[] = {
    { "cpu-c", "c-abi-runtime-handle",
      "C ABI/runtime call over an address-like DeviceSlot handle" },
    { "cpu-llvm", "llvm-ssa-address-space",
      "LLVM SSA value with target address-space and calling-convention metadata" },
    { "self-hosted", "owner-fact-artifact",
      "self-hosted owner-fact artifact consumed by a backend projection" },
};

/* One explicit target declaration keeps the physical boundary inspectable
 * while remaining honest about its status: this is a host-simulated device
 * window, not evidence that a particular board/MMU has been booted.  The
 * abstract operation table above remains the only operation SoT. */
static const PgyMachineLayerPhysicalGrant
PGY_HOST_SIM_DEVICE_GRANTS[] = {
    { "device-slot0", UINT64_C(0x10000000), UINT64_C(0x1000),
      PGY_MACHINE_LAYER_ACCESS_VOLATILE, true },
};

static const PgyMachineLayerPhysicalManifest PGY_HOST_SIM_PHYSICAL_MANIFEST = {
    PGY_MACHINE_LAYER_PHYSICAL_MANIFEST_ID,
    "host-sim-device",
    "host-sim-board",
    "host-sim-boot.v1",
    "host-sim-linker.v1",
    UINT64_C(0x7fffffff),
    "device-slot0",
    PGY_HOST_SIM_DEVICE_GRANTS,
    sizeof(PGY_HOST_SIM_DEVICE_GRANTS) / sizeof(PGY_HOST_SIM_DEVICE_GRANTS[0]),
    true
};

static const PgyMachineLayerPhysicalManifest *
PGY_ACTIVE_PHYSICAL_MANIFEST = &PGY_HOST_SIM_PHYSICAL_MANIFEST;

static const PgyMachineLayerTargetManifest PGY_ABSTRACT_DEVICE_SLOT_MANIFEST = {
    PGY_MACHINE_LAYER_MANIFEST_ID,
    "abstract-device-slot",
    PGY_MACHINE_OP_CLAIM
        | PGY_MACHINE_OP_READ
        | PGY_MACHINE_OP_WRITE
        | PGY_MACHINE_OP_RELEASE
        | PGY_MACHINE_OP_SUBMIT_READ,
    true,
    PGY_ABSTRACT_DEVICE_SLOT_OPERATIONS,
    sizeof(PGY_ABSTRACT_DEVICE_SLOT_OPERATIONS)
        / sizeof(PGY_ABSTRACT_DEVICE_SLOT_OPERATIONS[0]),
    PGY_ABSTRACT_DEVICE_SLOT_PROJECTIONS,
    sizeof(PGY_ABSTRACT_DEVICE_SLOT_PROJECTIONS)
        / sizeof(PGY_ABSTRACT_DEVICE_SLOT_PROJECTIONS[0])
};

const PgyMachineLayerTargetManifest *
pgy_machine_layer_target_manifest(void)
{
    return &PGY_ABSTRACT_DEVICE_SLOT_MANIFEST;
}

const PgyMachineLayerPhysicalManifest *
pgy_machine_layer_physical_manifest(void)
{
    return PGY_ACTIVE_PHYSICAL_MANIFEST;
}

bool
pgy_machine_layer_physical_manifest_bind(
    const PgyMachineLayerPhysicalManifest *manifest,
    const char **error_out)
{
    const PgyMachineLayerPhysicalManifest *active =
        pgy_machine_layer_physical_manifest();

    if (error_out != NULL)
        *error_out = NULL;
    if (!pgy_machine_layer_physical_manifest_validate(manifest, error_out))
        return false;
    if (active != &PGY_HOST_SIM_PHYSICAL_MANIFEST && active != manifest) {
        if (error_out != NULL)
            *error_out = "machine layer: physical declaration already bound";
        return false;
    }
    PGY_ACTIVE_PHYSICAL_MANIFEST = manifest;
    return true;
}

const char *
pgy_machine_layer_physical_access_mode_name(
    PgyMachineLayerPhysicalAccessMode mode)
{
    switch (mode) {
    case PGY_MACHINE_LAYER_ACCESS_PLAIN:
        return "plain";
    case PGY_MACHINE_LAYER_ACCESS_VOLATILE:
        return "volatile";
    case PGY_MACHINE_LAYER_ACCESS_ATOMIC:
        return "atomic";
    default:
        return "";
    }
}

static uint64_t
machine_layer_hash_bytes(uint64_t hash,
                         const unsigned char *bytes,
                         size_t count)
{
    for (size_t i = 0; i < count; i++) {
        hash ^= (uint64_t)bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t
machine_layer_hash_u64(uint64_t hash, uint64_t value)
{
    unsigned char bytes[sizeof(value)];
    for (size_t i = 0; i < sizeof(value); i++)
        bytes[i] = (unsigned char)(value >> (i * 8));
    return machine_layer_hash_bytes(hash, bytes, sizeof(bytes));
}

static uint64_t
machine_layer_hash_string(uint64_t hash, const char *value)
{
    if (value == NULL)
        return machine_layer_hash_u64(hash, UINT64_MAX);
    hash = machine_layer_hash_u64(hash, (uint64_t)strlen(value));
    return machine_layer_hash_bytes(
        hash, (const unsigned char *)value, strlen(value));
}

uint64_t
pgy_machine_layer_manifest_fingerprint(
    const PgyMachineLayerTargetManifest *manifest)
{
    uint64_t hash = UINT64_C(1469598103934665603);

    if (manifest == NULL)
        return 0;
    hash = machine_layer_hash_string(hash, manifest->manifest_id);
    hash = machine_layer_hash_string(hash, manifest->target_kind);
    hash = machine_layer_hash_u64(hash, manifest->supported_operations);
    hash = machine_layer_hash_u64(hash, manifest->hardware_adequate ? 1 : 0);
    hash = machine_layer_hash_u64(hash, manifest->operation_count);
    for (size_t i = 0; i < manifest->operation_count; i++) {
        const PgyMachineLayerOperationManifest *operation =
            manifest->operations != NULL ? &manifest->operations[i] : NULL;
        hash = machine_layer_hash_u64(
            hash, operation != NULL ? operation->operation : 0);
        hash = machine_layer_hash_string(
            hash, operation != NULL ? operation->contact_name : NULL);
        hash = machine_layer_hash_string(
            hash, operation != NULL ? operation->runtime_operation : NULL);
        hash = machine_layer_hash_u64(
            hash, operation != NULL && operation->requires_authority ? 1 : 0);
        hash = machine_layer_hash_u64(
            hash, operation != NULL && operation->requires_live_lease ? 1 : 0);
    }
    hash = machine_layer_hash_u64(hash, manifest->projection_count);
    for (size_t i = 0; i < manifest->projection_count; i++) {
        const PgyMachineLayerProjectionManifest *projection =
            manifest->projections != NULL ? &manifest->projections[i] : NULL;
        hash = machine_layer_hash_string(
            hash, projection != NULL ? projection->projection : NULL);
        hash = machine_layer_hash_string(
            hash, projection != NULL ? projection->physical_representation : NULL);
        hash = machine_layer_hash_string(
            hash, projection != NULL ? projection->lowering_contract : NULL);
    }
    return hash;
}

uint64_t
pgy_machine_layer_physical_manifest_fingerprint(
    const PgyMachineLayerPhysicalManifest *manifest)
{
    uint64_t hash = UINT64_C(1469598103934665603);

    if (manifest == NULL)
        return 0;
    hash = machine_layer_hash_string(hash, manifest->manifest_id);
    hash = machine_layer_hash_string(hash, manifest->target_kind);
    hash = machine_layer_hash_string(hash, manifest->board_id);
    hash = machine_layer_hash_string(hash, manifest->boot_contract);
    hash = machine_layer_hash_string(hash, manifest->linker_contract);
    hash = machine_layer_hash_u64(hash, manifest->address_limit);
    hash = machine_layer_hash_string(hash, manifest->device_grant_id);
    hash = machine_layer_hash_u64(hash, manifest->grant_count);
    hash = machine_layer_hash_u64(hash, manifest->hardware_adequate ? 1 : 0);
    for (size_t i = 0; i < manifest->grant_count; i++) {
        const PgyMachineLayerPhysicalGrant *grant =
            manifest->grants != NULL ? &manifest->grants[i] : NULL;
        hash = machine_layer_hash_string(
            hash, grant != NULL ? grant->grant_id : NULL);
        hash = machine_layer_hash_u64(hash, grant != NULL ? grant->base : 0);
        hash = machine_layer_hash_u64(hash, grant != NULL ? grant->size : 0);
        hash = machine_layer_hash_u64(
            hash, grant != NULL ? grant->mode : UINT64_MAX);
        hash = machine_layer_hash_u64(
            hash, grant != NULL && grant->hardware_adequate ? 1 : 0);
    }
    return hash;
}

bool
pgy_machine_layer_manifest_supports(
    const PgyMachineLayerTargetManifest *manifest,
    RIRMachineContactKind operation)
{
    return pgy_machine_layer_manifest_operation(manifest, operation) != NULL;
}

const PgyMachineLayerOperationManifest *
pgy_machine_layer_manifest_operation(
    const PgyMachineLayerTargetManifest *manifest,
    RIRMachineContactKind operation)
{
    if (manifest == NULL || manifest->operations == NULL)
        return NULL;
    for (size_t i = 0; i < manifest->operation_count; i++) {
        const PgyMachineLayerOperationManifest *entry =
            &manifest->operations[i];
        if (entry->operation == operation)
            return entry;
    }
    return NULL;
}

size_t
pgy_machine_layer_manifest_operation_count(
    const PgyMachineLayerTargetManifest *manifest)
{
    return manifest != NULL ? manifest->operation_count : 0;
}

const PgyMachineLayerOperationManifest *
pgy_machine_layer_manifest_operation_at(
    const PgyMachineLayerTargetManifest *manifest,
    size_t index)
{
    if (manifest == NULL || manifest->operations == NULL
        || index >= manifest->operation_count)
        return NULL;
    return &manifest->operations[index];
}

const PgyMachineLayerProjectionManifest *
pgy_machine_layer_manifest_projection(
    const PgyMachineLayerTargetManifest *manifest,
    const char *projection)
{
    if (manifest == NULL || manifest->projections == NULL
        || projection == NULL || projection[0] == '\0')
        return NULL;
    for (size_t i = 0; i < manifest->projection_count; i++) {
        const PgyMachineLayerProjectionManifest *entry =
            &manifest->projections[i];
        if (entry->projection != NULL
            && strcmp(entry->projection, projection) == 0)
            return entry;
    }
    return NULL;
}

static bool
pgy_machine_layer_projection_fail(const char **error_out,
                                  const char *message)
{
    if (error_out != NULL)
        *error_out = message;
    return false;
}

bool
pgy_machine_layer_projection_validate(
    const PgyMachineLayerTargetManifest *manifest,
    const char *projection,
    const char **error_out)
{
    const PgyMachineLayerProjectionManifest *row;

    if (error_out != NULL)
        *error_out = NULL;
    if (manifest == NULL)
        return pgy_machine_layer_projection_fail(
            error_out, "machine layer: missing target manifest");
    if (manifest->manifest_id == NULL
        || strcmp(manifest->manifest_id, PGY_MACHINE_LAYER_MANIFEST_ID) != 0)
        return pgy_machine_layer_projection_fail(
            error_out, "machine layer: unsupported manifest identity");
    if (!manifest->hardware_adequate)
        return pgy_machine_layer_projection_fail(
            error_out, "machine layer: hardware adequacy is not proven");
    if (manifest->operation_count == 0 || manifest->operations == NULL)
        return pgy_machine_layer_projection_fail(
            error_out, "machine layer: operation contract is empty");
    row = pgy_machine_layer_manifest_projection(manifest, projection);
    if (row == NULL)
        return pgy_machine_layer_projection_fail(
            error_out, "machine layer: backend projection is not admitted");
    if (row->physical_representation == NULL
        || row->physical_representation[0] == '\0'
        || row->lowering_contract == NULL
        || row->lowering_contract[0] == '\0')
        return pgy_machine_layer_projection_fail(
            error_out, "machine layer: backend projection is incomplete");
    return true;
}

bool
pgy_machine_layer_projection_ready_for_backend(
    const char *projection,
    const char **error_out)
{
    return pgy_machine_layer_projection_validate(
        pgy_machine_layer_target_manifest(), projection, error_out);
}

static bool
pgy_machine_layer_physical_fail(const char **error_out, const char *message)
{
    if (error_out != NULL)
        *error_out = message;
    return false;
}

const PgyMachineLayerPhysicalGrant *
pgy_machine_layer_physical_manifest_grant(
    const PgyMachineLayerPhysicalManifest *manifest,
    const char *grant_id)
{
    if (manifest == NULL || manifest->grants == NULL || grant_id == NULL)
        return NULL;
    for (size_t i = 0; i < manifest->grant_count; i++) {
        const PgyMachineLayerPhysicalGrant *grant = &manifest->grants[i];
        if (grant->grant_id != NULL && strcmp(grant->grant_id, grant_id) == 0)
            return grant;
    }
    return NULL;
}

bool
pgy_machine_layer_physical_manifest_validate(
    const PgyMachineLayerPhysicalManifest *manifest,
    const char **error_out)
{
    const PgyMachineLayerPhysicalGrant *device_grant;

    if (error_out != NULL)
        *error_out = NULL;
    if (manifest == NULL)
        return pgy_machine_layer_physical_fail(
            error_out, "machine layer: missing physical declaration");
    if (manifest->manifest_id == NULL
        || strncmp(manifest->manifest_id,
                   PGY_MACHINE_LAYER_PHYSICAL_MANIFEST_PREFIX,
                   strlen(PGY_MACHINE_LAYER_PHYSICAL_MANIFEST_PREFIX)) != 0
        || manifest->manifest_id[
               strlen(PGY_MACHINE_LAYER_PHYSICAL_MANIFEST_PREFIX)] == '\0')
        return pgy_machine_layer_physical_fail(
            error_out, "machine layer: unsupported physical declaration identity");
    if (manifest->target_kind == NULL || manifest->target_kind[0] == '\0')
        return pgy_machine_layer_physical_fail(
            error_out, "machine layer: physical target identity is missing");
    if (manifest->board_id == NULL || manifest->board_id[0] == '\0'
        || manifest->boot_contract == NULL
        || manifest->boot_contract[0] == '\0'
        || manifest->linker_contract == NULL
        || manifest->linker_contract[0] == '\0')
        return pgy_machine_layer_physical_fail(
            error_out, "machine layer: physical declaration provenance is missing");
    if (!manifest->hardware_adequate || manifest->grant_count == 0
        || manifest->grants == NULL || manifest->device_grant_id == NULL
        || manifest->device_grant_id[0] == '\0')
        return pgy_machine_layer_physical_fail(
            error_out, "machine layer: physical declaration is not adequate");
    for (size_t i = 0; i < manifest->grant_count; i++) {
        const PgyMachineLayerPhysicalGrant *grant = &manifest->grants[i];
        if (grant->grant_id == NULL || grant->grant_id[0] == '\0'
            || grant->size == 0 || grant->mode > PGY_MACHINE_LAYER_ACCESS_ATOMIC
            || !grant->hardware_adequate)
            return pgy_machine_layer_physical_fail(
                error_out, "machine layer: physical grant is incomplete");
        if (grant->base > manifest->address_limit
            || grant->size > manifest->address_limit - grant->base)
            return pgy_machine_layer_physical_fail(
                error_out, "machine layer: physical grant exceeds address limit");
    }
    for (size_t i = 0; i < manifest->grant_count; i++) {
        const PgyMachineLayerPhysicalGrant *grant = &manifest->grants[i];
        for (size_t j = i + 1; j < manifest->grant_count; j++) {
            const PgyMachineLayerPhysicalGrant *other = &manifest->grants[j];
            /* The first pass proved both end points are representable, so
             * these additions cannot wrap during interval comparison. */
            if (strcmp(grant->grant_id, other->grant_id) == 0)
                return pgy_machine_layer_physical_fail(
                    error_out, "machine layer: physical grant ids are not unique");
            if (grant->base < other->base + other->size
                && other->base < grant->base + grant->size)
                return pgy_machine_layer_physical_fail(
                    error_out, "machine layer: physical grants overlap");
        }
    }
    device_grant = pgy_machine_layer_physical_manifest_grant(
        manifest, manifest->device_grant_id);
    if (device_grant == NULL)
        return pgy_machine_layer_physical_fail(
            error_out, "machine layer: device grant is not declared");
    if (!device_grant->hardware_adequate
        || device_grant->mode != PGY_MACHINE_LAYER_ACCESS_VOLATILE)
        return pgy_machine_layer_physical_fail(
            error_out, "machine layer: device grant is not admitted");
    return true;
}

bool
pgy_machine_layer_physical_projection_ready_for_backend(
    const char *projection,
    const char **error_out)
{
    const char *physical_error = NULL;
    if (!pgy_machine_layer_projection_ready_for_backend(
            projection, error_out))
        return false;
    if (!pgy_machine_layer_physical_manifest_validate(
            pgy_machine_layer_physical_manifest(), &physical_error)) {
        if (error_out != NULL)
            *error_out = physical_error != NULL
                ? physical_error
                : "machine layer: physical declaration is not ready";
        return false;
    }
    if (pgy_machine_layer_physical_manifest_fingerprint(
            pgy_machine_layer_physical_manifest()) == 0)
        return pgy_machine_layer_physical_fail(
            error_out, "machine layer: physical declaration fingerprint is missing");
    return true;
}

bool
pgy_machine_layer_manifest_validate_site(
    const PgyMachineLayerTargetManifest *manifest,
    const PgyMachineLayerSiteFactView *site,
    const char **error_out)
{
    const PgyMachineLayerOperationManifest *operation = NULL;
    const PgyMachineLayerPhysicalManifest *physical_manifest;
    const PgyMachineLayerPhysicalGrant *physical_grant;

    if (error_out != NULL)
        *error_out = NULL;
    if (manifest == NULL || site == NULL)
        return pgy_machine_layer_projection_fail(
            error_out, "machine layer: missing site validation input");
    physical_manifest = pgy_machine_layer_physical_manifest();
    if (!pgy_machine_layer_physical_manifest_validate(
            physical_manifest, error_out))
        return false;
    if (manifest->manifest_id == NULL
        || site->manifest_id == NULL
        || strcmp(site->manifest_id, manifest->manifest_id) != 0)
        return pgy_machine_layer_projection_fail(
            error_out, "machine layer: site manifest identity is not admitted");
    if (site->contact_name == NULL || site->contact_name[0] == '\0')
        return pgy_machine_layer_projection_fail(
            error_out, "machine layer: site contact identity is missing");
    if (site->physical_grant_id == NULL || site->physical_grant_id[0] == '\0')
        return pgy_machine_layer_projection_fail(
            error_out, "machine layer: site physical grant identity is missing");
    physical_grant = pgy_machine_layer_physical_manifest_grant(
        physical_manifest, site->physical_grant_id);
    if (physical_grant == NULL || !physical_grant->hardware_adequate
        || physical_grant->mode != PGY_MACHINE_LAYER_ACCESS_VOLATILE)
        return pgy_machine_layer_projection_fail(
            error_out, "machine layer: site physical grant is not admitted");
    if (site->physical_base != physical_grant->base
        || site->physical_size != physical_grant->size
        || site->physical_mode != physical_grant->mode)
        return pgy_machine_layer_projection_fail(
            error_out,
            "machine layer: site physical declaration shape disagrees with owner");
    for (size_t i = 0; i < manifest->operation_count; i++) {
        const PgyMachineLayerOperationManifest *candidate =
            pgy_machine_layer_manifest_operation_at(manifest, i);
        if (candidate != NULL && candidate->contact_name != NULL
            && strcmp(candidate->contact_name, site->contact_name) == 0) {
            operation = candidate;
            break;
        }
    }
    if (operation == NULL)
        return pgy_machine_layer_projection_fail(
            error_out, "machine layer: site contact operation is unknown");
    if (site->runtime_operation == NULL
        || strcmp(site->runtime_operation,
                  operation->runtime_operation) != 0)
        return pgy_machine_layer_projection_fail(
            error_out, "machine layer: site runtime operation disagrees");
    if (site->hardware_adequate != manifest->hardware_adequate)
        return pgy_machine_layer_projection_fail(
            error_out, "machine layer: site hardware adequacy disagrees");
    if (site->authority_required != operation->requires_authority
        || site->live_lease_required != operation->requires_live_lease)
        return pgy_machine_layer_projection_fail(
            error_out, "machine layer: site proof requirements disagree");
    return true;
}

static void
machine_layer_json_write_string(FILE *out, const char *value)
{
    fputc('"', out);
    if (value != NULL) {
        for (const unsigned char *p = (const unsigned char *)value;
             *p != '\0'; p++) {
            switch (*p) {
            case '"': fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n", out); break;
            case '\r': fputs("\\r", out); break;
            case '\t': fputs("\\t", out); break;
            default:
                if (*p < 0x20)
                    fprintf(out, "\\u%04x", (unsigned)*p);
                else
                    fputc((int)*p, out);
                break;
            }
        }
    }
    fputc('"', out);
}

void
pgy_machine_layer_manifest_dump_json(FILE *out)
{
    const PgyMachineLayerTargetManifest *manifest =
        pgy_machine_layer_target_manifest();
    const PgyMachineLayerPhysicalManifest *physical =
        pgy_machine_layer_physical_manifest();
    uint64_t manifest_fingerprint =
        pgy_machine_layer_manifest_fingerprint(manifest);
    uint64_t physical_fingerprint =
        pgy_machine_layer_physical_manifest_fingerprint(physical);

    if (out == NULL)
        out = stdout;
    fputs("{\"schema\":\"pgy.machine-layer.declaration.v1\",\"manifest\":{",
          out);
    fputs("\"id\":", out);
    machine_layer_json_write_string(out,
        manifest != NULL ? manifest->manifest_id : NULL);
    fputs(",\"target_kind\":", out);
    machine_layer_json_write_string(out,
        manifest != NULL ? manifest->target_kind : NULL);
    fprintf(out, ",\"hardware_adequate\":%s,\"fingerprint\":%" PRIu64
                ",\"operations\":[",
            manifest != NULL && manifest->hardware_adequate ? "true" : "false",
            manifest_fingerprint);
    if (manifest != NULL) {
        for (size_t i = 0; i < manifest->operation_count; i++) {
            const PgyMachineLayerOperationManifest *operation =
                &manifest->operations[i];
            if (i > 0)
                fputc(',', out);
            fputs("{\"contact\":", out);
            machine_layer_json_write_string(out, operation->contact_name);
            fputs(",\"runtime_operation\":", out);
            machine_layer_json_write_string(out, operation->runtime_operation);
            fprintf(out,
                    ",\"requires_authority\":%s,\"requires_live_lease\":%s}",
                    operation->requires_authority ? "true" : "false",
                    operation->requires_live_lease ? "true" : "false");
        }
    }
    fputs("]},\"physical\":{\"id\":", out);
    machine_layer_json_write_string(out,
        physical != NULL ? physical->manifest_id : NULL);
    fputs(",\"target_kind\":", out);
    machine_layer_json_write_string(out,
        physical != NULL ? physical->target_kind : NULL);
    fputs(",\"board_id\":", out);
    machine_layer_json_write_string(out,
        physical != NULL ? physical->board_id : NULL);
    fputs(",\"boot_contract\":", out);
    machine_layer_json_write_string(out,
        physical != NULL ? physical->boot_contract : NULL);
    fputs(",\"linker_contract\":", out);
    machine_layer_json_write_string(out,
        physical != NULL ? physical->linker_contract : NULL);
    fprintf(out, ",\"address_limit\":%" PRIu64 ",\"device_grant\":",
            physical != NULL ? physical->address_limit : 0);
    machine_layer_json_write_string(out,
        physical != NULL ? physical->device_grant_id : NULL);
    fprintf(out, ",\"hardware_adequate\":%s,\"fingerprint\":%" PRIu64
                ",\"grants\":[",
            physical != NULL && physical->hardware_adequate ? "true" : "false",
            physical_fingerprint);
    if (physical != NULL) {
        for (size_t i = 0; i < physical->grant_count; i++) {
            const PgyMachineLayerPhysicalGrant *grant = &physical->grants[i];
            if (i > 0)
                fputc(',', out);
            fputs("{\"id\":", out);
            machine_layer_json_write_string(out, grant->grant_id);
            fprintf(out, ",\"base\":%" PRIu64 ",\"size\":%" PRIu64
                        ",\"mode\":", grant->base, grant->size);
            machine_layer_json_write_string(out,
                pgy_machine_layer_physical_access_mode_name(grant->mode));
            fprintf(out, ",\"hardware_adequate\":%s}",
                    grant->hardware_adequate ? "true" : "false");
        }
    }
    fputs("]}}\n", out);
}

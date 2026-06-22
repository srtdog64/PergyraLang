#ifndef PGY_PKG_MANIFEST_H
#define PGY_PKG_MANIFEST_H

#include <stdbool.h>

#include "driver_app.h"

typedef struct
{
    char *seashell_schema;
    char *seashell_format;
    char *name;
    char *version;
    char *pergyra;
    char *edition;
    char *entry;
    char *test_entry;
    BackendKind backend;
    bool backend_set;
    bool deterministic;
    bool deterministic_set;
} PgyPackageManifest;

void pgy_package_manifest_destroy(PgyPackageManifest *manifest);
bool pgy_package_manifest_load(PgyPackageManifest *manifest, const char *path);
bool pgy_package_manifest_verify_existing_lock(
    const PgyPackageManifest *manifest,
    const char *path);
int pgy_package_manifest_write_lock(const PgyPackageManifest *manifest);
char *pgy_package_manifest_entry_path_dup(
    const PgyPackageManifest *manifest,
    bool test_target);
const char *pgy_package_manifest_backend_name(BackendKind backend);
bool pgy_package_manifest_package_name_is_valid(const char *name);
const char *pgy_package_manifest_template(void);

#endif /* PGY_PKG_MANIFEST_H */

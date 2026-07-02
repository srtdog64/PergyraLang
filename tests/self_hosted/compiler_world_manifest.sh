#!/usr/bin/env bash
# Canonical path fact for the self-hosted compiler world. Keep this file small:
# it is the shell-side projection of StagePathManifest in
# src/self_hosted/compiler/world.pgy.

PGY_SELFHOST_SOURCE_DIR="src/self_hosted"
PGY_SELFHOST_TEST_DIR="tests/self_hosted"
PGY_SELFHOST_PARITY_DIR="tests/self_hosted/parity"
PGY_SELFHOST_COMPILER_WORLD_PATH="src/self_hosted/compiler/world.pgy"
PGY_SELFHOST_COMPILER_PATH_MANIFEST_PATH="src/self_hosted/compiler/path_manifest_owner.pgy"
PGY_SELFHOST_COMPILER_STAGE_INTENTS_PATH="src/self_hosted/compiler/stage_intents.pgy"
PGY_SELFHOST_COMPILER_TARGET_CAPABILITY_PATH="src/self_hosted/compiler/target_capability_owner.pgy"
PGY_SELFHOST_COMPILER_AIR_EVIDENCE_PATH="src/self_hosted/compiler/air_evidence_owner.pgy"
PGY_SELFHOST_COMPILER_ARTIFACT_ZONE_PATH="src/self_hosted/compiler/artifact_zone_owner.pgy"
PGY_SELFHOST_COMPILER_TEST_HARNESS_PATH="src/self_hosted/compiler/test_harness_owner.pgy"
PGY_SELFHOST_COMPILER_SUBPROCESS_RUNNER_PATH="src/self_hosted/compiler/subprocess_runner_owner.pgy"
PGY_SELFHOST_COMPILER_ABI_LAYOUT_ROW_PATH="src/self_hosted/compiler/abi_layout_row_owner.pgy"
PGY_SELFHOST_COMPILER_SYMBOL_TABLE_PATH="src/self_hosted/compiler/symbol_table_owner.pgy"
PGY_SELFHOST_COMPILER_STAGE_ARTIFACT_PATH="src/self_hosted/compiler/stage_artifact_owner.pgy"
PGY_SELFHOST_COMPILER_AUTHORITY_PATH="src/self_hosted/compiler/authority_owner.pgy"

PGY_SELFHOST_STAGE_PATHS=(
    "src/self_hosted/lexer/main.pgy"
    "src/self_hosted/parser/main.pgy"
    "src/self_hosted/semantic/main.pgy"
    "src/self_hosted/mir_lower/main.pgy"
    "src/self_hosted/codegen/main.pgy"
)

PGY_SELFHOST_PARITY_PATHS=(
    "tests/self_hosted/parity/lexer_parity.sh"
    "tests/self_hosted/parity/parser_parity.sh"
    "tests/self_hosted/parity/semantic_parity.sh"
    "tests/self_hosted/parity/mir_json_parity.sh"
    "tests/self_hosted/parity/codegen_parity.sh"
    "tests/self_hosted/parity/codegen_bootstrap.sh"
)

PGY_SELFHOST_COMPILER_WORLD_MANIFEST_PATHS=(
    "$PGY_SELFHOST_COMPILER_WORLD_PATH"
    "$PGY_SELFHOST_COMPILER_PATH_MANIFEST_PATH"
    "$PGY_SELFHOST_COMPILER_STAGE_INTENTS_PATH"
    "$PGY_SELFHOST_COMPILER_TARGET_CAPABILITY_PATH"
    "$PGY_SELFHOST_COMPILER_AIR_EVIDENCE_PATH"
    "$PGY_SELFHOST_COMPILER_ARTIFACT_ZONE_PATH"
    "$PGY_SELFHOST_COMPILER_TEST_HARNESS_PATH"
    "$PGY_SELFHOST_COMPILER_SUBPROCESS_RUNNER_PATH"
    "$PGY_SELFHOST_COMPILER_ABI_LAYOUT_ROW_PATH"
    "$PGY_SELFHOST_COMPILER_SYMBOL_TABLE_PATH"
    "$PGY_SELFHOST_COMPILER_STAGE_ARTIFACT_PATH"
    "$PGY_SELFHOST_COMPILER_AUTHORITY_PATH"
    "src/self_hosted/OWNERS.md"
    "${PGY_SELFHOST_STAGE_PATHS[@]}"
    "${PGY_SELFHOST_PARITY_PATHS[@]}"
)

pgy_compiler_world_require_manifest_paths() {
    local root="$1"
    local rel

    [[ -d "$root/$PGY_SELFHOST_SOURCE_DIR" ]] || return 1
    [[ -d "$root/$PGY_SELFHOST_TEST_DIR" ]] || return 1
    [[ -d "$root/$PGY_SELFHOST_PARITY_DIR" ]] || return 1

    for rel in "${PGY_SELFHOST_COMPILER_WORLD_MANIFEST_PATHS[@]}"; do
        [[ -f "$root/$rel" ]] || return 1
    done
}

# Conformance: bind the compiler world to the on-disk stage owners so the
# architecture manifest cannot silently drift from reality.
#  - forward: every manifest stage owns a real dir with .pgy facts AND is named
#    as a `<stage>: String` field in StagePathManifest (world.pgy);
#  - reverse: every src/self_hosted/<stage>/main.pgy is a named manifest stage,
#    so a new stage dir cannot appear without the world naming it.
pgy_compiler_world_require_stage_conformance() {
    local root="$1"
    local world="$root/$PGY_SELFHOST_COMPILER_WORLD_PATH"
    local stage_path stage_dir stage_name main_pgy name
    local -a stage_names=()

    [[ -f "$world" ]] || return 1

    for stage_path in "${PGY_SELFHOST_STAGE_PATHS[@]}"; do
        stage_dir="$(dirname "$stage_path")"
        stage_name="$(basename "$stage_dir")"
        stage_names+=("$stage_name")
        [[ -d "$root/$stage_dir" ]] || return 1
        compgen -G "$root/$stage_dir/*.pgy" >/dev/null || return 1
        grep -Fq -- "$stage_name: String" "$world" || return 1
    done

    while IFS= read -r main_pgy; do
        name="$(basename "$(dirname "$main_pgy")")"
        printf '%s\n' "${stage_names[@]}" | grep -qx -- "$name" || return 1
    done < <(find "$root/$PGY_SELFHOST_SOURCE_DIR" -mindepth 2 -maxdepth 2 \
                 -name main.pgy)
}

#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

grep -Fq "inherited_who_from_intent" \
    "$ROOT_DIR/src/parser/ast_domain_data.h"
grep -Fq "inherited_where_from_intent" \
    "$ROOT_DIR/src/parser/ast_domain_data.h"
grep -Fq "inherited_who_from_intent = false" \
    "$ROOT_DIR/src/parser/ast_domain_constructors.c"
grep -Fq "inherited_where_from_intent = false" \
    "$ROOT_DIR/src/parser/ast_domain_constructors.c"
grep -Fq "inherited_who_from_intent = copied_any" \
    "$ROOT_DIR/src/parser/parser_intent.c"
grep -Fq "inherited_where_from_intent =" \
    "$ROOT_DIR/src/parser/parser_intent.c"
grep -Fq "reused who from intent-level default" \
    "$ROOT_DIR/src/parser/ast_print_intent.c"
grep -Fq "reused zone from intent-level default" \
    "$ROOT_DIR/src/parser/ast_print_intent.c"
grep -Fq "reused who from intent-level default" \
    "$ROOT_DIR/src/semantic/type_checker_intent_contract_summary.c"
grep -Fq "reused zone from intent-level default" \
    "$ROOT_DIR/src/semantic/type_checker_intent_contract_summary.c"
grep -Fq "inherited from the intent-level who default" \
    "$ROOT_DIR/src/semantic/type_checker_intent_participants.c"
grep -Fq "intent_step_where_source_label" \
    "$ROOT_DIR/src/semantic/type_checker_intent_decl.c"
grep -Fq "this where value came from %s" \
    "$ROOT_DIR/src/semantic/type_checker_intent_decl.c"
grep -Fq "compressed using derivation can only bind a named intent participant or value" \
    "$ROOT_DIR/src/semantic/type_checker_intent_decl.c"
grep -Fq "using binding points to a different zone than the current where contract" \
    "$ROOT_DIR/src/semantic/type_checker_intent_decl.c"
grep -Fq "compressed intent orchestration needs a stable declared intent target" \
    "$ROOT_DIR/src/semantic/type_checker_intent_decl.c"
grep -Fq "non-intent callees do not carry intent step provenance into AIR" \
    "$ROOT_DIR/src/semantic/type_checker_intent_decl.c"
grep -Fq "transfer target derivation and explicit using binding choose different zones" \
    "$ROOT_DIR/src/semantic/type_checker_intent_transfer.c"
grep -Fq "explicit or inherited step zone conflicts with the transfer destination" \
    "$ROOT_DIR/src/semantic/type_checker_intent_transfer.c"
grep -Fq "who_inherited_from_intent" \
    "$ROOT_DIR/src/compiler/dir.h"
grep -Fq "where_inherited_from_intent" \
    "$ROOT_DIR/src/compiler/dir.h"
grep -Fq "who-default=intent" \
    "$ROOT_DIR/src/compiler/dir_validate.c"
grep -Fq "where-default=intent" \
    "$ROOT_DIR/src/compiler/dir_validate.c"
grep -Fq "who_from_intent_default" \
    "$ROOT_DIR/src/compiler/air.h"
grep -Fq "source_from_intent_default" \
    "$ROOT_DIR/src/compiler/air.h"
grep -Fq "source_from_transfer" \
    "$ROOT_DIR/src/compiler/air.h"
grep -Fq "source_provenance=" \
    "$ROOT_DIR/src/compiler/air_verify.c"
grep -Fq "who_provenance=" \
    "$ROOT_DIR/src/compiler/air_verify.c"
grep -Fq "intent-default+transfer" \
    "$ROOT_DIR/src/compiler/air_verify.c"
grep -Fq "AIR world boundary node %zu has no transfer provenance" \
    "$ROOT_DIR/src/compiler/air_validate.c"
grep -Fq "test_air_verify_rejects_world_boundary_without_transfer_provenance" \
    "$ROOT_DIR/src/tests/air/test_air_strict_part_f.cases.h"
grep -Fq "AIR verify rejects world boundary without transfer provenance" \
    "$ROOT_DIR/src/test_air.c"
grep -Fq "who_from_intent_default" \
    "$ROOT_DIR/src/compiler/air_dump.c"
grep -Fq "source_from_intent_default" \
    "$ROOT_DIR/src/compiler/air_dump.c"
grep -Fq "source_from_transfer" \
    "$ROOT_DIR/src/compiler/air_dump.c"
grep -Fq "intent step reuses intent-level who and where defaults" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_d.cases.h"
grep -Fq "intent-level who default failure reports provenance" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_d.cases.h"
grep -Fq "intent-level where default failure reports provenance" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_g.cases.h"
grep -Fq "this where value came from the intent-level where default" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_g.cases.h"
grep -Fq "intent using mismatch reports compressed derivation reason" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_g.cases.h"
grep -Fq "using binding points to a different zone than the current where contract" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_g.cases.h"
grep -Fq "non-intent callees do not carry intent step provenance into AIR" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_d.cases.h"
grep -Fq "declare the participant with 'who ghost: <Subject>;'" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_d.cases.h"
grep -Fq "inherited_who_from_intent" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_d.cases.h"
grep -Fq "inherited_where_from_intent" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_d.cases.h"
grep -Fq ".who_inherited_from_intent = true" \
    "$ROOT_DIR/src/tests/air/test_air_core_part_a.cases.h"
grep -Fq ".where_inherited_from_intent = true" \
    "$ROOT_DIR/src/tests/air/test_air_core_part_a.cases.h"
grep -Fq "air->intents[0].who_from_intent_default" \
    "$ROOT_DIR/src/tests/air/test_air_core_part_a.cases.h"
grep -Fq "air->boundaries[0].source_from_intent_default" \
    "$ROOT_DIR/src/tests/air/test_air_core_part_a.cases.h"
grep -Fq "air->boundaries[0].source_from_transfer" \
    "$ROOT_DIR/src/tests/air/test_air_cleanup_transfer_part_c.cases.h"

if grep -F "locally declared who on step" \
        "$ROOT_DIR/src/semantic/type_checker_intent_contract_summary.c" \
    | grep -Fq "inherited_who_from_intent"; then
    :
else
    grep -Fq "&& !step->data.intent_step.inherited_who_from_intent" \
        "$ROOT_DIR/src/semantic/type_checker_intent_contract_summary.c"
fi

grep -Fq "&& !step->data.intent_step.inherited_where_from_intent" \
    "$ROOT_DIR/src/semantic/type_checker_intent_contract_summary.c"

echo "[intent-compression-contract] intent-level who/where provenance is source-gated"

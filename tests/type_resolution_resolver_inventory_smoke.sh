#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

bad_direct="$(mktemp)"
bad_fallback="$(mktemp)"
fallback_matches="$(mktemp)"
annotation_matches="$(mktemp)"
bad_annotation="$(mktemp)"
annotation_or_unknown_matches="$(mktemp)"
bad_annotation_or_unknown="$(mktemp)"
bad_record="$(mktemp)"
type_ref_helper_matches="$(mktemp)"
trap 'rm -f "$bad_direct" "$bad_fallback" "$fallback_matches" "$annotation_matches" "$bad_annotation" "$annotation_or_unknown_matches" "$bad_annotation_or_unknown" "$bad_record" "$type_ref_helper_matches"' EXIT

{ grep -RIn "resolve_type_node(" src/semantic || true; } | while IFS=: read -r path line text; do
  case "$path" in
    src/semantic/type_checker_resolution_metadata.c)
      continue
      ;;
  esac

  trimmed="$(printf '%s' "$text" | sed 's/^[[:space:]]*//')"
  case "$trimmed" in
    *'resolve_type_node("T")'*)
      continue
      ;;
    '/*'*|'\*'*)
      continue
      ;;
  esac

  printf '%s:%s: %s\n' "$path" "$line" "$trimmed" >>"$bad_direct"
done

if [ -s "$bad_direct" ]; then
  echo "[type-resolution-resolver-inventory] unexpected direct resolver call(s):" >&2
  cat "$bad_direct" >&2
  echo "Move new calls behind an owner-local resolver seam or explicitly document the compatibility fallback allowlist." >&2
  exit 1
fi

if grep -q 'resolve_type_node(ASTNode' src/semantic/type_checker.h; then
  echo "[type-resolution-resolver-inventory] public type_checker.h re-exposed resolve_type_node" >&2
  exit 1
fi

if grep -RIn --include='*.c' --include='*.h' 'domain_resolve_type_ref(' src/semantic >"$bad_direct"; then
  echo "[type-resolution-resolver-inventory] broad domain resolver API reappeared:" >&2
  cat "$bad_direct" >&2
  echo "Use metadata-backed owner seams such as domain_lookup_slot_type_metadata(...) or semantic_type_resolution_lookup_metadata_type_ref(...)." >&2
  exit 1
fi

: >"$bad_direct"
{ grep -RIn 'find_domain_decl_by_name(' src/semantic || true; } | while IFS=: read -r path line text; do
  [ -n "$path" ] || continue
  if [ "$path" = "src/semantic/type_checker_decls_domain_helpers.c" ] ||
     [ "$path" = "src/semantic/type_checker_host_helpers.c" ] ||
     [ "$path" = "src/semantic/type_checker_host_lookup.c" ] ||
     [ "$path" = "src/semantic/type_checker_internal.h" ]; then
    continue
  fi
  printf '%s:%s: %s\n' "$path" "$line" "$text" >>"$bad_direct"
done

if [ -s "$bad_direct" ]; then
  echo "[type-resolution-resolver-inventory] broad domain declaration lookup escaped its owner seam:" >&2
  cat "$bad_direct" >&2
  echo "Use semantic_find_*_decl_by_name(...) or an intent/zone/world owner seam instead of reopening find_domain_decl_by_name(...)." >&2
  exit 1
fi

if grep -RInE '(^|[^A-Za-z0-9_])find_domain_decl_by_name\(' src/semantic >/dev/null; then
  echo "[type-resolution-resolver-inventory] raw domain declaration lookup helper reappeared" >&2
  echo "Keep domain declaration lookup private to SemanticContext-backed host/domain owner seams." >&2
  exit 1
fi

if grep -q 'ASTNode \*program' src/semantic/type_checker_ability_match_internal.h; then
  echo "[type-resolution-resolver-inventory] ability matching header re-exposed program-root based helpers" >&2
  exit 1
fi

if grep -RIn 'role_decl_has_ability(' src/semantic \
  | grep -v 'semantic_role_decl_has_ability' \
  | grep -v 'role_decl_has_ability_rec' >"$bad_direct"; then
  echo "[type-resolution-resolver-inventory] role ability matching reopened a program-root helper:" >&2
  cat "$bad_direct" >&2
  echo "Use semantic_role_decl_has_ability(...) so declaration lookup stays behind SemanticContext owner seams." >&2
  exit 1
fi

if grep -RIn 'ability_ref_matches(' src/semantic \
  | grep -v 'semantic_ability_ref_matches' >"$bad_direct"; then
  echo "[type-resolution-resolver-inventory] ability matching reopened a raw program-root ability-ref helper:" >&2
  cat "$bad_direct" >&2
  echo "Use the SemanticContext-backed ability matcher owner seam instead." >&2
  exit 1
fi

grep -q 'semantic_host_index_find_next_decl_of_type' \
  src/semantic/type_checker_host_index.c || {
  echo "[type-resolution-resolver-inventory] host declaration index must own typed declaration iteration" >&2
  exit 1
}

grep -q 'semantic_host_index_find_next_decl_of_type' \
  src/semantic/type_checker_domain_role_lookup.c || {
  echo "[type-resolution-resolver-inventory] role lookup must consume host-index declaration iteration before program-root compatibility" >&2
  exit 1
}

grep -q 'AST_ROLE_DECL' \
  src/semantic/type_checker_domain_role_lookup.c || {
  echo "[type-resolution-resolver-inventory] role lookup host-index iteration must be role-typed" >&2
  exit 1
}

if grep -RIn 'resolve_projection_source_field_path(ASTNode' src/semantic >"$bad_direct"; then
  echo "[type-resolution-resolver-inventory] projection path resolver re-exposed a program-root API:" >&2
  cat "$bad_direct" >&2
  echo "Use semantic_resolve_projection_source_field_path(...) so projection lookup stays behind SemanticContext/metadata seams." >&2
  exit 1
fi

if [ -e src/semantic/type_checker_resolve.c ] || [ -e src/semantic/type_checker_resolve.h ]; then
  echo "[type-resolution-resolver-inventory] obsolete type_checker_resolve compatibility owner reappeared" >&2
  exit 1
fi

grep -q 'Retired compatibility resolver quarantine' \
  src/semantic/type_checker_resolution_retired.c || {
  echo "[type-resolution-resolver-inventory] retired compatibility quarantine owner missing" >&2
  exit 1
}

if grep -q 'g_type_resolution_compat_' src/semantic/type_checker_resolution_retired.c src/semantic/type_checker_internal.h; then
  echo "[type-resolution-resolver-inventory] retired compatibility zero-only counters reappeared" >&2
  exit 1
fi

grep -q 'require_assignable(Type \*from, Type \*to' \
  src/semantic/type_checker_type_helpers.c || {
  echo "[type-resolution-resolver-inventory] assignability helper did not move out of retired resolver owner" >&2
  exit 1
}

test_direct="$(
  grep -RIn "resolve_type_node(" src/tests src/test_semantic.c 2>/dev/null || true
)"
if [ -n "$test_direct" ]; then
  echo "[type-resolution-resolver-inventory] semantic regression test reintroduced direct resolver call(s):" >&2
  printf '%s\n' "$test_direct" >&2
  echo "Use semantic_type_resolution_lookup_metadata_type_ref(...) so DAG tests exercise the metadata-only API." >&2
  exit 1
fi

grep -RIn 'semantic_type_resolution_resolve_or_fallback' src/semantic \
  | grep -v 'type_checker_internal.h' >"$fallback_matches" || true

while IFS=: read -r path line text; do
  [ -n "$path" ] || continue
  if [ "$path" = "src/semantic/type_checker_program.c" ] ||
     [ "$path" = "src/semantic/type_checker_resolution_metadata.c" ]; then
    continue
  fi
  printf '%s:%s: %s\n' "$path" "$line" "$text" >>"$bad_fallback"
done <"$fallback_matches"

if [ -s "$bad_fallback" ]; then
  echo "[type-resolution-resolver-inventory] unclassified metadata-first fallback seam(s):" >&2
  cat "$bad_fallback" >&2
  echo "Classify new fallback users as graph-backed owner seams or explicit compatibility seams before landing." >&2
  exit 1
fi

fallback_sites="$(
  wc -l <"$fallback_matches"
)"
if [ "$fallback_sites" -gt 0 ]; then
  echo "[type-resolution-resolver-inventory] metadata-first fallback seam inventory grew: $fallback_sites > 0" >&2
  echo "Shrink or explicitly justify the fallback owner allowlist before adding new seams." >&2
  exit 1
fi

if grep -RIn 'resolve_type_node(' src/semantic \
  | grep -v 'type_checker_resolution_metadata.c' >"$bad_direct"; then
  echo "[type-resolution-resolver-inventory] retired resolve_type_node evaluator reappeared" >&2
  cat "$bad_direct" >&2
  exit 1
fi

if grep -q 'find_domain_decl_by_name' src/semantic/type_checker_resolution_stage_domain.c; then
  echo "[type-resolution-resolver-inventory] DAG domain stage must consume semantic domain lookup seams" >&2
  exit 1
fi

if grep -q 'ast_program_statement_count(program)' src/semantic/type_checker_resolution_helpers.c; then
  echo "[type-resolution-resolver-inventory] type-alias lookup must consume the host declaration index, not program-root scans" >&2
  exit 1
fi

{ grep -RIn 'resolve_named_type(' src/semantic || true; } \
  >"$bad_direct" || true

if [ -s "$bad_direct" ]; then
  echo "[type-resolution-resolver-inventory] retired direct named-type resolver reappeared:" >&2
  cat "$bad_direct" >&2
  echo "Use DAG metadata lookup seams such as semantic_type_resolution_lookup_metadata_name_or_alias(...)." >&2
  exit 1
fi

{ grep -RIn 'semantic_type_resolution_lookup_resolved_type' src/semantic || true; } \
  | grep -Ev 'src/semantic/type_checker_resolution_metadata(_alias|_constructed)?\.c' \
  | grep -Ev 'src/semantic/type_checker_resolution_metadata_internal\.h' \
  >"$bad_annotation" || true

if [ -s "$bad_annotation" ]; then
  echo "[type-resolution-resolver-inventory] raw resolved-type lookup escaped metadata owners:" >&2
  cat "$bad_annotation" >&2
  echo "Use semantic_type_resolution_lookup_annotation_nullable(...) inside metadata owners or metadata-first type-ref helpers elsewhere." >&2
  exit 1
fi

{ grep -RIn 'semantic_type_resolution_lookup_resolved_annotation' src/semantic || true; } \
  >"$annotation_matches" || true

if [ -s "$annotation_matches" ]; then
  echo "[type-resolution-resolver-inventory] retired resolved-annotation API reappeared:" >&2
  cat "$annotation_matches" >&2
  echo "Use semantic_type_resolution_lookup_annotation_nullable(...) inside metadata owners or metadata-first type-ref helpers elsewhere." >&2
  exit 1
fi

annotation_sites="$(wc -l <"$annotation_matches")"
if [ "$annotation_sites" -ne 0 ]; then
  echo "[type-resolution-resolver-inventory] resolved-annotation seam inventory changed: $annotation_sites != 0" >&2
  echo "Use metadata-first type-ref helpers; the annotation-or-unknown API is retired." >&2
  cat "$annotation_matches" >&2
  exit 1
fi

{ grep -RIn 'semantic_type_resolution_lookup_annotation_or_unknown' src/semantic || true; } \
  >"$annotation_or_unknown_matches" || true

cp "$annotation_or_unknown_matches" "$bad_annotation_or_unknown"

if [ -s "$bad_annotation_or_unknown" ]; then
  echo "[type-resolution-resolver-inventory] unclassified annotation-only type-ref read(s):" >&2
  cat "$bad_annotation_or_unknown" >&2
  echo "Contract/boundary declaration paths should use metadata-first type-ref helpers; annotation-only reads must stay private to metadata owners." >&2
  exit 1
fi

annotation_or_unknown_count="$(wc -l <"$annotation_or_unknown_matches")"
if [ "$annotation_or_unknown_count" -ne 0 ]; then
  echo "[type-resolution-resolver-inventory] annotation-only type-ref read inventory changed: $annotation_or_unknown_count != 0" >&2
  cat "$annotation_or_unknown_matches" >&2
  exit 1
fi

annotation_nullable_consumers="$(
  grep -RIn 'semantic_type_resolution_lookup_annotation_nullable' src/semantic \
    | grep -Ev 'src/semantic/type_checker_internal\.h' \
    | grep -Ev 'src/semantic/type_checker_resolution_metadata\.c' \
    || true
)"
if [ -n "$annotation_nullable_consumers" ]; then
  echo "[type-resolution-resolver-inventory] unclassified nullable annotation read(s):" >&2
  echo "$annotation_nullable_consumers" >&2
  echo "Boundary/contract paths should use semantic_type_resolution_lookup_metadata_type_ref(...); keep nullable reads private to metadata owners." >&2
  exit 1
fi

{ grep -RIn 'semantic_type_resolution_record_\(owned_\)\?resolved_type' src/semantic || true; } \
  | grep -Ev 'src/semantic/type_checker_internal\.h' \
  | grep -Ev 'src/semantic/type_checker_resolution_internal\.h' \
  | grep -Ev 'src/semantic/type_checker_resolution_graph_core\.c' \
  | grep -Ev 'src/semantic/type_checker_resolution_stage_signature\.c' \
  | grep -Ev 'src/semantic/type_checker_resolution_metadata(_alias|_constructed)?\.c' \
  >"$bad_record" || true

if [ -s "$bad_record" ]; then
  echo "[type-resolution-resolver-inventory] resolved-type metadata recorder escaped DAG owners:" >&2
  cat "$bad_record" >&2
  echo "Only graph/stage-signature/metadata materialization owners may write DAG resolved-type facts." >&2
  exit 1
fi

if grep -q 'resolve_type_node_legacy_quarantined\|resolve_type_node_uncached' \
  src/semantic/type_checker_resolution_retired.c; then
  echo "[type-resolution-resolver-inventory] retired resolver compatibility body reappeared" >&2
  exit 1
fi

grep -q 'semantic_type_resolution_try_record_stable_constructed_type(ctx, type_node)' \
  src/semantic/type_checker_resolution_metadata.c || {
  echo "[type-resolution-resolver-inventory] metadata type-ref no longer materializes stable constructed types before resolver fallback" >&2
  exit 1
}

grep -q 'recursive resolver fallback is retired' \
  src/semantic/type_checker_resolution_metadata_dead_end.c || {
  echo "[type-resolution-resolver-inventory] fallback diagnostic no longer explains the retired recursive resolver boundary" >&2
  exit 1
}

grep -q 'PGY_TYPE_RES_DEAD_END_TRACE' \
  src/semantic/type_checker_resolution_metadata_dead_end.c || {
  echo "[type-resolution-resolver-inventory] metadata dead-end trace env var regressed to fallback naming" >&2
  exit 1
}

grep -q 'type_resolution_metadata_dead_ends' \
  src/semantic/semantic.h || {
  echo "[type-resolution-resolver-inventory] semantic result does not expose dead-end DAG evidence count" >&2
  exit 1
}

grep -q 'dead_ends=%llu' \
  src/semantic/type_checker_program_stats.c || {
  echo "[type-resolution-resolver-inventory] type-resolution stats do not expose dead_ends" >&2
  exit 1
}

if grep -RIn 'type_resolution_metadata_materializer_fallbacks\|materializer_fallbacks=%llu' \
  src/semantic; then
  echo "[type-resolution-resolver-inventory] materializer_fallbacks compatibility mirror reappeared" >&2
  exit 1
fi

if grep -RIn 'type_resolution_metadata_fallback_' src/semantic; then
  echo "[type-resolution-resolver-inventory] DAG dead-end family counters regressed to fallback-era naming" >&2
  exit 1
fi

grep -q 'type_resolution_metadata_unresolved_named' \
  src/semantic/type_checker.h || {
  echo "[type-resolution-resolver-inventory] DAG dead-end family counters no longer use unresolved naming" >&2
  exit 1
}

grep -q 'program_lookup_dag_type_ref_or_unknown' \
  src/semantic/type_checker_program.c || {
  echo "[type-resolution-resolver-inventory] program placeholder path lost explicit DAG metadata type-ref lookup seam" >&2
  exit 1
}

if grep -q 'program_resolve_type_quiet\|program_resolve_func_return_type_quiet\|program_resolve_intent_binding_type_quiet' \
  src/semantic/type_checker_program.c; then
  echo "[type-resolution-resolver-inventory] program placeholder path reintroduced resolver-style naming" >&2
  exit 1
fi

if grep -q 'semantic_type_resolution_lookup_or_materialize(ctx' \
  src/semantic/type_checker_program.c; then
  echo "[type-resolution-resolver-inventory] program placeholder path must not materialize metadata locally" >&2
  exit 1
fi

grep -q 'semantic_result_type_resolution_metadata_dead_ends' \
  src/compiler/air_evidence_dag.c || {
  echo "[type-resolution-resolver-inventory] AIR evidence must consume SemanticResult DAG accessors" >&2
  exit 1
}

if grep -q 'sem->type_resolution_' src/compiler/air_evidence_dag.c; then
  echo "[type-resolution-resolver-inventory] AIR DAG evidence reopens raw SemanticResult DAG counters" >&2
  exit 1
fi

grep -q '\[type-res-dead-end\]' \
  src/semantic/type_checker_resolution_metadata_dead_end.c || {
  echo "[type-resolution-resolver-inventory] metadata dead-end trace label regressed to fallback naming" >&2
  exit 1
}

if grep -q 'PGY_TYPE_RES_FALLBACK_TRACE\|\[type-res-fallback\]' \
  src/semantic/type_checker_resolution_metadata_dead_end.c; then
  echo "[type-resolution-resolver-inventory] metadata dead-end trace still uses fallback naming" >&2
  exit 1
fi

grep -q 'owner-local metadata materializer' \
  src/semantic/type_checker_resolution_metadata_dead_end.c || {
  echo "[type-resolution-resolver-inventory] fallback diagnostic no longer points at the owner-local materializer fix" >&2
  exit 1
}

grep -q 'resolved = semantic_type_resolution_lookup_metadata_type_ref(ctx, type_node)' \
  src/semantic/type_checker_resolution_stage_signature.c || {
  echo "[type-resolution-resolver-inventory] signature stage no longer consumes metadata type-ref before returning unknown" >&2
  exit 1
}

grep -q 'semantic_type_resolution_lookup_metadata_type_ref' \
  src/semantic/type_checker_resolution_metadata.c || {
  echo "[type-resolution-resolver-inventory] central metadata type-ref lookup owner disappeared" >&2
  exit 1
}

grep -q 'semantic_type_resolution_lookup_metadata_name_or_alias_or_unknown' \
  src/semantic/type_checker_resolution_metadata_diagnostics.c || {
  echo "[type-resolution-resolver-inventory] metadata diagnostics lost central name-or-alias unknown-type helper" >&2
  exit 1
}

for local_unknown_helper in \
  expr_resolve_named_type_metadata_or_unknown \
  expr_host_resolve_named_type_metadata_or_unknown \
  overlay_resolve_named_type_metadata_or_unknown; do
  if grep -RIn "$local_unknown_helper" src/semantic; then
    echo "[type-resolution-resolver-inventory] owner-local unknown-type metadata helper reappeared: $local_unknown_helper" >&2
    exit 1
  fi
done

grep -q 'semantic_type_resolution_lookup_metadata_type_ref' \
  src/semantic/type_checker_ownership_let_helpers.c || {
  echo "[type-resolution-resolver-inventory] ownership let type-ref path no longer consumes DAG metadata first" >&2
  exit 1
}

grep -q 'semantic_type_resolution_reject_invalid_stable_constructed_type' \
  src/semantic/type_checker_ownership_let_helpers.c || {
  echo "[type-resolution-resolver-inventory] ownership let type-ref path lost stable constructed-type diagnostics" >&2
  exit 1
}

grep -q 'semantic_type_resolution_reject_unknown_bare_named_type' \
  src/semantic/type_checker_ownership_let_helpers.c || {
  echo "[type-resolution-resolver-inventory] ownership let type-ref path lost unknown bare-name diagnostics" >&2
  exit 1
}

grep -q 'semantic_type_resolution_record_type_ref_dependency' \
  src/semantic/type_checker_ability_where.c || {
  echo "[type-resolution-resolver-inventory] ability where-bound validation lost DAG dependency provenance" >&2
  exit 1
}

grep -q 'ability consumer lookup' \
  src/semantic/type_checker_ability_where.c || {
  echo "[type-resolution-resolver-inventory] ability where-bound validation lost consumer-path provenance label" >&2
  exit 1
}

grep -q 'collect_effective_generic_arg_types' \
  src/semantic/type_checker_ability_where.c || {
  echo "[type-resolution-resolver-inventory] ability where-bound validation lost effective generic argument materialization" >&2
  exit 1
}

grep -q 'semantic_report_ability_generic_bound_failure' \
  src/semantic/type_checker_ability_where.c || {
  echo "[type-resolution-resolver-inventory] ability where-bound validation lost rich mismatch provenance diagnostic" >&2
  exit 1
}

grep -q 'zone authority ability consumer lookup' \
  src/semantic/type_checker_zone_decl_authority.c || {
  echo "[type-resolution-resolver-inventory] zone authority validation lost DAG ability-consumer provenance label" >&2
  exit 1
}

grep -q 'validate_ability_decl_where_clause_reference' \
  src/semantic/type_checker_module_contract.c || {
  echo "[type-resolution-resolver-inventory] required ability resolver lost ability where-bound enforcement" >&2
  exit 1
}

grep -q 'resolve_required_ability_decl' \
  src/semantic/type_checker_zone_decl_authority.c || {
  echo "[type-resolution-resolver-inventory] zone authority validation no longer goes through required ability resolver" >&2
  exit 1
}

grep -q 'intent_find_zone_decl_for_type' \
  src/semantic/type_checker_intent_types.c || {
  echo "[type-resolution-resolver-inventory] intent zone declaration recovery lost its owner seam" >&2
  exit 1
}

grep -q 'intent_find_effect_decl_by_name' \
  src/semantic/type_checker_intent_types.c || {
  echo "[type-resolution-resolver-inventory] intent effect declaration recovery lost its owner seam" >&2
  exit 1
}

grep -q 'intent_resolve_step_where_zone_decl' \
  src/semantic/type_checker_intent_step_sequence.c || {
  echo "[type-resolution-resolver-inventory] intent step where validation no longer consumes the zone owner seam" >&2
  exit 1
}

if grep -n 'AST_\(ZONE\|EFFECT\)_DECL' \
  src/semantic/type_checker_intent_decl.c \
  src/semantic/type_checker_intent_binding_context.c \
  src/semantic/type_checker_intent_participants.c \
  src/semantic/type_checker_intent_transfer.c; then
  echo "[type-resolution-resolver-inventory] intent where/using/causes/transfer consumers must use the intent domain owner seam" >&2
  exit 1
fi

{ grep -RIn 'semantic_type_resolution_lookup_type_ref_or_materialize' src/semantic || true; } \
  >"$type_ref_helper_matches" || true

type_ref_helper_count="$(wc -l <"$type_ref_helper_matches")"
if [ "$type_ref_helper_count" -ne 0 ]; then
  echo "[type-resolution-resolver-inventory] metadata-first type-ref helper inventory changed: $type_ref_helper_count != 0" >&2
  cat "$type_ref_helper_matches" >&2
  exit 1
fi

direct_metadata_type_ref_users="$(
  grep -RIn 'semantic_type_resolution_lookup_metadata_type_ref(ctx' src/semantic \
    | grep -Ev 'src/semantic/type_checker_generic_contracts\.c' \
    | grep -Ev 'src/semantic/type_checker_generic_effective_args\.c' \
    | grep -Ev 'src/semantic/type_checker_generic_validation\.c' \
    | grep -Ev 'src/semantic/type_checker_program\.c' \
    | grep -Ev 'src/semantic/type_checker_expr\.c' \
    | grep -Ev 'src/semantic/type_checker_expr_lambda\.c' \
    | grep -Ev 'src/semantic/type_checker_expr_enum\.c' \
    | grep -Ev 'src/semantic/type_checker_ability_decl\.c' \
    | grep -Ev 'src/semantic/type_checker_decls_domain_helpers\.c' \
    | grep -Ev 'src/semantic/type_checker_func_types\.c' \
    | grep -Ev 'src/semantic/type_checker_host_helpers\.c' \
    | grep -Ev 'src/semantic/type_checker_intent_types\.c' \
    | grep -Ev 'src/semantic/type_checker_ownership_let_helpers\.c' \
    | grep -Ev 'src/semantic/type_checker_projection_path\.c' \
    | grep -Ev 'src/semantic/type_checker_type_alias\.c' \
    | grep -Ev 'src/semantic/type_checker_flow\.c' \
    | grep -Ev 'src/semantic/type_checker_world_helpers\.c' \
    | grep -Ev 'src/semantic/type_checker_resolution_metadata\.c' \
    | grep -Ev 'src/semantic/type_checker_resolution_metadata_constructed\.c' \
    | grep -Ev 'src/semantic/type_checker_resolution_metadata_diagnostics\.c' \
    | grep -Ev 'src/semantic/type_checker_resolution_stage_alias\.c' \
    | grep -Ev 'src/semantic/type_checker_resolution_stage_domain_decl\.c' \
    | grep -Ev 'src/semantic/type_checker_resolution_stage_signature\.c' \
    | grep -Ev 'src/semantic/type_checker_resolution_stage_nominal\.c' \
    | grep -Ev 'src/semantic/type_checker_resolution_stage_systemic\.c' \
    || true
)"
if [ -n "$direct_metadata_type_ref_users" ]; then
  echo "[type-resolution-resolver-inventory] unclassified direct metadata type-ref consumer(s):" >&2
  echo "$direct_metadata_type_ref_users" >&2
  exit 1
fi

direct_materializer_users="$(
  grep -RIn 'semantic_type_resolution_lookup_or_materialize(ctx' src/semantic \
    | grep -Ev 'src/semantic/type_checker_resolution_metadata\.c' \
    || true
)"
if [ -n "$direct_materializer_users" ]; then
  echo "[type-resolution-resolver-inventory] semantic owner reopened direct metadata materializer:" >&2
  echo "$direct_materializer_users" >&2
  exit 1
fi

direct_metadata_materializer_count="$(
  grep -c 'semantic_type_resolution_lookup_or_materialize(ctx' \
    src/semantic/type_checker_resolution_metadata.c || true
)"
if [ "$direct_metadata_materializer_count" != "0" ]; then
  echo "[type-resolution-resolver-inventory] central metadata owner must not keep type-ref materializer fallback calls (found $direct_metadata_materializer_count)" >&2
  exit 1
fi

if grep -RIn 'semantic_stage_record_compat_family' src/semantic; then
  echo "[type-resolution-resolver-inventory] retired stage compatibility-family recorder reappeared" >&2
  exit 1
fi

if grep -q 'semantic_type_resolution_lookup_type_ref_or_materialize' \
  src/semantic/type_checker_resolution_stage_signature.c; then
  echo "[type-resolution-resolver-inventory] stage signature reintroduced materializer fallback" >&2
  exit 1
fi

if grep -q 'semantic_type_resolution_lookup_type_ref_or_materialize' \
  src/semantic/type_checker_resolution_metadata_diagnostics.c; then
  echo "[type-resolution-resolver-inventory] metadata diagnostics reintroduced materializer lookup" >&2
  exit 1
fi

if grep -nE '(^|[^A-Za-z0-9_])(args_node|decl_params|gp)->(count|params|name|constraint|default_type)\b' \
  src/semantic/type_checker_resolution_metadata_constructed.c; then
  echo "[type-resolution-resolver-inventory] constructed metadata owner reopened GenericParams storage directly" >&2
  exit 1
fi

grep -q 'ast_generic_param_count' \
  src/semantic/type_checker_resolution_metadata_constructed.c || {
  echo "[type-resolution-resolver-inventory] constructed metadata owner lost GenericParams accessor seam" >&2
  exit 1
}

for generic_owner in \
  src/semantic/type_checker_generic_support.c \
  src/semantic/type_checker_generic_contracts.c \
  src/semantic/type_checker_ability_ref.c \
  src/semantic/type_checker_ability_match.c \
  src/semantic/type_checker_ability_where.c; do
  if grep -nE '(^|[^A-Za-z0-9_])(params|provided_args|decl_params|generic_args|gp|lhs_args|rhs_args|decl_gp|provided_gp)->(count|params|name|constraint|default_type)\b' \
    "$generic_owner"; then
    echo "[type-resolution-resolver-inventory] $generic_owner reopened GenericParams storage directly" >&2
    exit 1
  fi
done

grep -q 'ast_generic_param_count' \
  src/semantic/type_checker_generic_support.c || {
  echo "[type-resolution-resolver-inventory] generic support lost GenericParams accessor seam" >&2
  exit 1
}

if grep -nE 'ast_type_generic_args\([^)]*\)->(count|params)\b' \
  src/semantic/type_checker_generic_contracts.c; then
  echo "[type-resolution-resolver-inventory] generic contracts reopened type generic-args storage directly" >&2
  exit 1
fi

if grep -nE '(^|[^A-Za-z0-9_])(gp|param)->(count|params|name|constraint|default_type)\b' \
  src/semantic/type_checker_generic_validation.c; then
  echo "[type-resolution-resolver-inventory] generic validation reopened GenericParams storage directly" >&2
  exit 1
fi

grep -q 'ast_generic_param_default_type' \
  src/semantic/type_checker_generic_validation.c || {
  echo "[type-resolution-resolver-inventory] generic validation lost GenericParams accessor seam" >&2
  exit 1
}

for generic_decl_owner in \
  src/semantic/type_checker_ability_decl.c \
  src/semantic/type_checker_class_decl.c \
  src/semantic/type_checker_func_decl.c \
  src/semantic/type_checker_call_generic_where.c; do
  if grep -nE '(^|[^A-Za-z0-9_])(ability_generics|class_generics|func_generics|decl_gp|gp)->(count|params|name|constraint|default_type)\b' \
    "$generic_decl_owner"; then
    echo "[type-resolution-resolver-inventory] $generic_decl_owner reopened GenericParams storage directly" >&2
    exit 1
  fi
done

for generic_contract_owner in \
  src/semantic/type_checker_module_contract.c \
  src/semantic/type_checker_intent_role_fields.c \
  src/semantic/type_checker_resolution_graph_collect.c \
  src/semantic/type_checker_party_decl.c \
  src/semantic/type_checker_call_constructor.c \
  src/semantic/type_checker_role_decl.c \
  src/semantic/type_checker_roster_decl.c \
  src/semantic/type_checker_helpers_late.c; do
  if grep -nE '(^|[^A-Za-z0-9_])(ability_generics|ability_args|generic_args|generic_params|class_generics|role_generics|included_generics|impl_type_args|decl_params|callable_generic_params|gp|param)->(count|params|name|constraint|default_type)\b' \
    "$generic_contract_owner"; then
    echo "[type-resolution-resolver-inventory] $generic_contract_owner reopened GenericParams storage directly" >&2
    exit 1
  fi
done

for generic_stage_owner in \
  src/semantic/type_checker_resolution_stage_nominal.c \
  src/semantic/type_checker_resolution_stage_signature.c \
  src/semantic/type_checker_resolution_metadata.c \
  src/semantic/type_checker_resolution_metadata_alias.c \
  src/semantic/type_checker_resolution_metadata_dead_end.c \
  src/semantic/type_checker_resolution_metadata_diagnostics.c \
  src/semantic/type_checker_resolution_graph_decl.c; do
  if grep -nE '(^|[^A-Za-z0-9_])(gp|generic_params|class_generics|args|args_node|type_args|arg)->(count|params|name|constraint|default_type)\b' \
    "$generic_stage_owner"; then
    echo "[type-resolution-resolver-inventory] $generic_stage_owner reopened GenericParams storage directly" >&2
    exit 1
  fi
done

if grep -nE 'ast_type_generic_args\([^)]*\)->(count|params)\b' \
  src/semantic/type_checker_resolution_metadata.c \
  src/semantic/type_checker_resolution_metadata_alias.c \
  src/semantic/type_checker_resolution_metadata_dead_end.c \
  src/semantic/type_checker_resolution_metadata_diagnostics.c; then
  echo "[type-resolution-resolver-inventory] metadata owner reopened type generic-args storage directly" >&2
  exit 1
fi

if grep -nE 'ast_type_generic_args\([^)]*\)->(count|params)\b' \
  src/semantic/type_checker_module_contract.c \
  src/semantic/type_checker_resolution_graph_collect.c; then
  echo "[type-resolution-resolver-inventory] generic contract/graph owner reopened type generic-args storage directly" >&2
  exit 1
fi

if grep -RInE 'ast_type_generic_args\([^)]*\)->(count|params)|[A-Za-z0-9_]+->params\[[^]]+\]|[A-Za-z0-9_]+->default_type' \
  src/semantic; then
  echo "[type-resolution-resolver-inventory] semantic reopened GenericParams storage directly" >&2
  exit 1
fi

if grep -RInE 'ast_type_generic_args\([^)]*\)->(count|params)|(^|[^A-Za-z0-9_])(generic_args|generic_params|ability_generics|return_generic_args|resolved_generic_args|gp|ga)->(count|params)|->default_type|->constraint' \
  src/compiler src/codegen; then
  echo "[type-resolution-resolver-inventory] compiler/codegen reopened GenericParams storage directly" >&2
  exit 1
fi

grep -q 'typedef struct TypeNameSlot' \
  src/semantic/type_checker_resolution_metadata.c || {
  echo "[type-resolution-resolver-inventory] metadata builtin/shell lookup lost dispatch-table entry type" >&2
  exit 1
}

grep -q 'bool keep_owned = owned;' \
  src/semantic/type_checker_resolution_metadata.c || {
  echo "[type-resolution-resolver-inventory] metadata update lost ownership preservation state" >&2
  exit 1
}

grep -q 'ctx->type_resolution_metadata.owned\[i\] == resolved_type' \
  src/semantic/type_checker_resolution_metadata.c && {
  echo "[type-resolution-resolver-inventory] metadata ownership preservation compares owned bit to type pointer" >&2
  exit 1
}

grep -q 'ctx->type_resolution_metadata.values\[i\] == resolved_type' \
  src/semantic/type_checker_resolution_metadata.c || {
  echo "[type-resolution-resolver-inventory] metadata update may downgrade ownership on same-pointer re-record" >&2
  exit 1
}

grep -q 'static const TypeNameSlot builtins' \
  src/semantic/type_checker_resolution_metadata.c || {
  echo "[type-resolution-resolver-inventory] metadata builtin lookup reintroduced ad-hoc branches" >&2
  exit 1
}

grep -q 'metadata_type_name_slot_compare' \
  src/semantic/type_checker_resolution_metadata.c || {
  echo "[type-resolution-resolver-inventory] metadata builtin/shell lookup lost dispatch-table comparator" >&2
  exit 1
}

grep -q '&name, builtins' \
  src/semantic/type_checker_resolution_metadata.c || {
  echo "[type-resolution-resolver-inventory] metadata builtin lookup no longer uses dispatch-table search" >&2
  exit 1
}

grep -q 'static const TypeNameSlot shells' \
  src/semantic/type_checker_resolution_metadata.c || {
  echo "[type-resolution-resolver-inventory] metadata shell lookup reintroduced ad-hoc branches" >&2
  exit 1
}

grep -q '&name, shells' \
  src/semantic/type_checker_resolution_metadata.c || {
  echo "[type-resolution-resolver-inventory] metadata shell lookup no longer uses dispatch-table search" >&2
  exit 1
}

if grep -q 'strcmp(name, "' src/semantic/type_checker_resolution_helpers.c; then
  echo "[type-resolution-resolver-inventory] resolve_named_type reintroduced local builtin/shell name table" >&2
  exit 1
fi

if [ -e src/semantic/type_checker_resolution_helpers.h ]; then
  echo "[type-resolution-resolver-inventory] retired type_checker_resolution_helpers.h reappeared" >&2
  exit 1
fi

materializer_recorders="$(
  grep -RIn 'semantic_type_resolution_record_metadata_dead_end_diagnostic' src/semantic \
    | grep -v 'type_checker_internal.h' \
    | grep -v 'type_checker_resolution_metadata_dead_end.c' \
    | grep -v 'type_checker_resolution_metadata.c' || true
)"
if [ -n "$materializer_recorders" ]; then
  echo "[type-resolution-resolver-inventory] metadata dead-end diagnostic escaped central owner:" >&2
  printf '%s\n' "$materializer_recorders" >&2
  exit 1
fi

if grep -RIn 'semantic_type_resolution_record_materializer_fallback' src/semantic; then
  echo "[type-resolution-resolver-inventory] retired materializer fallback recorder name reappeared" >&2
  exit 1
fi

metadata_fallback_escape_count="$(
  grep -RIn 'resolve_type_node(type_node, ctx)' src/semantic/type_checker_resolution_metadata.c \
    | wc -l || true
)"
if [ "$metadata_fallback_escape_count" -ne 0 ]; then
  echo "[type-resolution-resolver-inventory] central metadata fallback escape hatch reappeared: $metadata_fallback_escape_count" >&2
  exit 1
fi

alias_stack_debt="$(
  grep -RIn 'alias_resolution_\|resolve_type_alias_decl' src/semantic \
    | grep -v 'type_resolution_resolver_inventory_smoke.sh' || true
)"
if [ -n "$alias_stack_debt" ]; then
  echo "[type-resolution-resolver-inventory] recursive alias resolver debt reappeared:" >&2
  printf '%s\n' "$alias_stack_debt" >&2
  exit 1
fi

for needle in \
  'typedef struct StableShellSpec' \
  'metadata_stable_shell_spec' \
  '{ "Channel", &TYPE_CHANNEL, 1, 1, false }' \
  '{ "Future", &TYPE_FUTURE, 1, 1, false }' \
  '{ "DeviceSlot", &TYPE_DEVICE_SLOT, 1, 1, false }' \
  '{ "Slot", NULL, 1, 1, true }' \
  '{ "SecureSlot", NULL, 1, 1, true }' \
  'typedef struct StableSlotShellSpec' \
  'stable_slot_shell_create' \
  'type_node->type == AST_EVENT_HANDLER_TYPE' \
  'type_create_function(param_types, param_count, return_type)' \
  'ast_type_tuple_element_count(type_node) > 0' \
  'type_create_tuple(elements, element_count)' \
  'TYPE_LONG' \
  'TYPE_BOOL'; do
  grep -q "$needle" \
    src/semantic/type_checker_resolution_metadata.c \
    src/semantic/type_checker_resolution_metadata_constructed.c \
    src/semantic/type_checker_resolution_metadata_diagnostics.c || {
    echo "[type-resolution-resolver-inventory] metadata materializer missing: $needle" >&2
    exit 1
  }
done

grep -q 'semantic_type_resolution_metadata_stable_constructed_shell(' \
  src/semantic/type_checker_resolution_metadata_constructed.c || {
  echo "[type-resolution-resolver-inventory] constructed metadata owner stopped using centralized stable shell vocabulary" >&2
  exit 1
}

grep -q 'find_intent_value_local' src/semantic/type_checker_intent_binding_context.c || {
  echo "[type-resolution-resolver-inventory] intent compressed using derivation no longer resolves value bindings" >&2
  exit 1
}

grep -q 'intent_binding_context_resolve_binding_type' src/semantic/type_checker_intent_binding_context.c || {
  echo "[type-resolution-resolver-inventory] intent using derivation must use the shared binding type resolver" >&2
  exit 1
}

grep -q 'semantic_find_zone_decl_by_name(ctx, within_zone)' \
  src/semantic/type_checker_func_action_contract.c || {
  echo "[type-resolution-resolver-inventory] action contract must resolve zones through the semantic domain owner seam" >&2
  exit 1
}

grep -q 'semantic_find_effect_decl_by_name(ctx, causes_effect)' \
  src/semantic/type_checker_func_action_contract.c || {
  echo "[type-resolution-resolver-inventory] action contract must resolve effects through the semantic domain owner seam" >&2
  exit 1
}

if grep -q 'find_domain_decl_by_name(' src/semantic/type_checker_func_action_contract.c; then
  echo "[type-resolution-resolver-inventory] action contract reintroduced direct domain declaration lookup" >&2
  exit 1
fi

grep -q 'semantic_find_effect_decl_by_name(ctx,' \
  src/semantic/type_checker_domain_contracts.c || {
  echo "[type-resolution-resolver-inventory] zone effect contract must resolve effects through the semantic domain owner seam" >&2
  exit 1
}

grep -q 'semantic_find_relation_decl_by_name(ctx,' \
  src/semantic/type_checker_domain_contracts.c || {
  echo "[type-resolution-resolver-inventory] zone relation contract must resolve relations through the semantic domain owner seam" >&2
  exit 1
}

if grep -q 'find_domain_decl_by_name(' src/semantic/type_checker_domain_contracts.c; then
  echo "[type-resolution-resolver-inventory] zone contract reintroduced direct domain declaration lookup" >&2
  exit 1
fi

grep -q 'semantic_find_zone_decl_by_name(ctx, zone_type)' \
  src/semantic/type_checker_world_decl.c || {
  echo "[type-resolution-resolver-inventory] world declaration must resolve zone declarations through the semantic domain owner seam" >&2
  exit 1
}

grep -q 'semantic_find_zone_decl_by_name(ctx, zone_type)' \
  src/semantic/type_checker_world_helpers.c || {
  echo "[type-resolution-resolver-inventory] world helper must resolve zone declarations through the semantic domain owner seam" >&2
  exit 1
}

grep -q 'semantic_find_world_decl_by_name(ctx, world_name)' \
  src/semantic/type_checker_world_embedding.c || {
  echo "[type-resolution-resolver-inventory] world embedding must resolve world declarations through the semantic domain owner seam" >&2
  exit 1
}

for world_consumer in \
  src/semantic/type_checker_world_decl.c \
  src/semantic/type_checker_world_helpers.c \
  src/semantic/type_checker_world_embedding.c; do
  if grep -q 'find_domain_decl_by_name(' "$world_consumer"; then
    echo "[type-resolution-resolver-inventory] world consumer reintroduced direct domain declaration lookup: $world_consumer" >&2
    exit 1
  fi
done

grep -q 'semantic_find_relation_decl_by_name(ctx, type_name)' \
  src/semantic/type_checker_zone_decl_authority.c || {
  echo "[type-resolution-resolver-inventory] zone layer slot relation lookup must use the semantic domain owner seam" >&2
  exit 1
}

grep -q 'semantic_find_effect_decl_by_name(ctx, type_name)' \
  src/semantic/type_checker_zone_decl_authority.c || {
  echo "[type-resolution-resolver-inventory] zone layer slot effect lookup must use the semantic domain owner seam" >&2
  exit 1
}

if grep -q 'find_domain_decl_by_name(' src/semantic/type_checker_zone_decl_authority.c; then
  echo "[type-resolution-resolver-inventory] zone authority consumer reintroduced direct domain declaration lookup" >&2
  exit 1
fi

grep -q 'semantic_find_world_decl_by_name(ctx, host_name)' \
  src/semantic/type_checker_resolution_stage_lookup.c || {
  echo "[type-resolution-resolver-inventory] graph host lookup must resolve worlds through the semantic domain owner seam" >&2
  exit 1
}

grep -q 'semantic_find_zone_decl_by_name(ctx, host_name)' \
  src/semantic/type_checker_resolution_stage_lookup.c || {
  echo "[type-resolution-resolver-inventory] graph host lookup must resolve zones through the semantic domain owner seam" >&2
  exit 1
}

if grep -q 'find_domain_decl_by_name(' src/semantic/type_checker_resolution_stage_lookup.c; then
  echo "[type-resolution-resolver-inventory] graph host lookup reintroduced direct domain declaration lookup" >&2
  exit 1
fi

for stage_signature_term in \
  'semantic_find_class_decl_by_name(ctx, provider_name)' \
  'semantic_find_ability_decl_by_name(ctx, provider_name)' \
  'semantic_find_role_decl_by_name(ctx, provider_name)' \
  'semantic_find_party_decl_by_name(ctx, provider_name)' \
  'semantic_find_roster_decl_by_name(ctx, provider_name)' \
  'semantic_find_world_decl_by_name(ctx, provider_name)' \
  'semantic_find_zone_decl_by_name(ctx, provider_name)' \
  'semantic_find_relation_decl_by_name(ctx, provider_name)' \
  'semantic_find_effect_decl_by_name(ctx, provider_name)'; do
  grep -q "$stage_signature_term" \
    src/semantic/type_checker_resolution_stage_signature.c || {
    echo "[type-resolution-resolver-inventory] stage signature domain lookup must use semantic owner seam: $stage_signature_term" >&2
    exit 1
  }
done

if grep -q 'find_domain_decl_by_name(' src/semantic/type_checker_resolution_stage_signature.c; then
  echo "[type-resolution-resolver-inventory] stage signature reintroduced direct domain declaration lookup" >&2
  exit 1
fi

if grep -q 'semantic_find_top_level_decl_by_label(ctx->program_root' \
  src/semantic/type_checker_resolution_stage_signature.c; then
  echo "[type-resolution-resolver-inventory] stage signature must not pass raw program root to top-level lookup" >&2
  exit 1
fi

for metadata_class_lookup_term in \
  'semantic_find_class_decl_by_name(ctx, provider_name)' \
  'semantic_find_class_decl_by_name(ctx, ast_type_name(type_node))' \
  'semantic_find_class_decl_by_name(ctx, name)'; do
  grep -R -q "$metadata_class_lookup_term" \
    src/semantic/type_checker_resolution_graph_core.c \
    src/semantic/type_checker_resolution_graph_domain.c \
    src/semantic/type_checker_resolution_metadata.c \
    src/semantic/type_checker_resolution_metadata_alias.c \
    src/semantic/type_checker_resolution_metadata_constructed.c \
    src/semantic/type_checker_resolution_metadata_dead_end.c || {
    echo "[type-resolution-resolver-inventory] metadata class lookup must use semantic owner seam: $metadata_class_lookup_term" >&2
    exit 1
  }
done

if grep -R -q 'find_type_decl_by_name(ctx->program_root' \
  src/semantic/type_checker_resolution_graph_core.c \
  src/semantic/type_checker_resolution_graph_domain.c \
  src/semantic/type_checker_resolution_metadata.c \
  src/semantic/type_checker_resolution_metadata_alias.c \
  src/semantic/type_checker_resolution_metadata_constructed.c \
  src/semantic/type_checker_resolution_metadata_dead_end.c; then
  echo "[type-resolution-resolver-inventory] metadata owners reintroduced raw class declaration lookup" >&2
  exit 1
fi

for metadata_alias_lookup_term in \
  'semantic_find_type_alias_decl_by_name(ctx, provider_name)' \
  'semantic_find_type_alias_decl_by_name(ctx, name)'; do
  grep -R -q "$metadata_alias_lookup_term" \
    src/semantic/type_checker_resolution_graph_core.c \
    src/semantic/type_checker_resolution_metadata_alias.c \
    src/semantic/type_checker_resolution_metadata_dead_end.c \
    src/semantic/type_checker_resolution_metadata_diagnostics.c || {
    echo "[type-resolution-resolver-inventory] metadata alias lookup must use semantic owner seam: $metadata_alias_lookup_term" >&2
    exit 1
  }
done

if grep -R -q 'find_type_alias_decl(ctx->program_root' \
  src/semantic/type_checker_resolution_graph_core.c \
  src/semantic/type_checker_resolution_metadata_alias.c \
  src/semantic/type_checker_resolution_metadata_dead_end.c \
  src/semantic/type_checker_resolution_metadata_diagnostics.c; then
  echo "[type-resolution-resolver-inventory] metadata owners reintroduced raw type-alias declaration lookup" >&2
  exit 1
fi

grep -q 'semantic_type_resolution_metadata_stable_builtin_shell_arity(' \
  src/semantic/type_checker_resolution_metadata_dead_end.c || {
  echo "[type-resolution-resolver-inventory] metadata fallback owner stopped using centralized stable shell arity" >&2
  exit 1
}

grep -q 'Type-resolution DAG could not materialize type metadata' \
  src/semantic/type_checker_resolution_metadata_dead_end.c || {
  echo "[type-resolution-resolver-inventory] metadata fallback no longer emits an explicit DAG diagnostic" >&2
  exit 1
}

for needle in \
  'case AST_CHANNEL_TYPE:' \
  'case AST_FUTURE_TYPE:' \
  'case AST_EVENT_HANDLER_TYPE:' \
  'semantic_type_resolution_try_record_stable_constructed_type(ctx, type_node)' \
  'ast_type_tuple_element_count(type_node) > 0'; do
  grep -q "$needle" src/semantic/type_checker_resolution_graph_collect.c || {
    echo "[type-resolution-resolver-inventory] graph collect materializer coverage missing: $needle" >&2
    exit 1
  }
done

echo "[type-resolution-resolver-inventory] direct resolver and fallback seam inventory are gated (fallback seams=$fallback_sites cap=0 annotation-sensitive seams=$annotation_sites annotation-only reads=$annotation_or_unknown_count cap=0 nullable annotation reads=0 type-ref helper refs=$type_ref_helper_count cap=0)"

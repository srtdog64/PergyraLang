#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

bad_direct="$(mktemp)"
bad_fallback="$(mktemp)"
fallback_matches="$(mktemp)"
trap 'rm -f "$bad_direct" "$bad_fallback" "$fallback_matches"' EXIT

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

if [ -e src/semantic/type_checker_resolve.c ] || [ -e src/semantic/type_checker_resolve.h ]; then
  echo "[type-resolution-resolver-inventory] obsolete type_checker_resolve compatibility owner reappeared" >&2
  exit 1
fi

grep -q 'Retired compatibility resolver audit counters' \
  src/semantic/type_checker_resolution_retired.c || {
  echo "[type-resolution-resolver-inventory] retired compatibility counter owner missing" >&2
  exit 1
}

grep -q 'size_t g_type_resolution_compat_calls = 0;' \
  src/semantic/type_checker_resolution_retired.c || {
  echo "[type-resolution-resolver-inventory] retired compatibility call counter missing" >&2
  exit 1
}

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
  echo "Use semantic_type_resolution_lookup_type_ref_or_materialize(...) so DAG tests exercise the metadata-first API." >&2
  exit 1
fi

grep -RIn 'semantic_type_resolution_resolve_or_fallback' src/semantic \
  | grep -v 'type_checker_internal.h' >"$fallback_matches" || true

while IFS=: read -r path line text; do
  [ -n "$path" ] || continue
  case "$path" in
    src/semantic/type_checker_program.c|\
    src/semantic/type_checker_resolution_metadata.c)
      continue
      ;;
  esac
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

grep -q 'resolved = semantic_type_resolution_lookup_metadata_type_ref(ctx, type_node)' \
  src/semantic/type_checker_resolution_stage_signature.c || {
  echo "[type-resolution-resolver-inventory] signature stage no longer consumes metadata type-ref before compatibility fallback" >&2
  exit 1
}

grep -q 'semantic_type_resolution_lookup_metadata_type_ref(ctx,' \
  src/semantic/type_checker_resolution_metadata.c || {
  echo "[type-resolution-resolver-inventory] type-ref-or-materialize helper lost metadata preflight" >&2
  exit 1
}

grep -q 'resolved = semantic_type_resolution_lookup_type_ref_or_materialize(ctx, type_node)' \
  src/semantic/type_checker_resolution_stage_signature.c || {
  echo "[type-resolution-resolver-inventory] signature stage no longer routes materialization through metadata-first type-ref helper" >&2
  exit 1
}

for owner in \
  src/semantic/type_checker_ability_where.c \
  src/semantic/type_checker_async_channel.h \
  src/semantic/type_checker_call_constructor.c \
  src/semantic/type_checker_call_generic_where.c \
  src/semantic/type_checker_class_decl.c \
  src/semantic/type_checker_decls_domain_helpers.c \
  src/semantic/type_checker_expr.c \
  src/semantic/type_checker_expr_call.c \
  src/semantic/type_checker_expr_host.c \
  src/semantic/type_checker_func_action_contract.c \
  src/semantic/type_checker_func_decl.c \
  src/semantic/type_checker_generic_contracts.h \
  src/semantic/type_checker_generic_support.h \
  src/semantic/type_checker_generic_validation.c \
  src/semantic/type_checker_helpers_late.c \
  src/semantic/type_checker_host_helpers.c \
  src/semantic/type_checker_intent_action_contract.c \
  src/semantic/type_checker_intent_decl.c \
  src/semantic/type_checker_intent_participants.c \
  src/semantic/type_checker_intent_role_fields.c \
  src/semantic/type_checker_intent_transfer.c \
  src/semantic/type_checker_operator_expr.h \
  src/semantic/type_checker_ownership_destructure.c \
  src/semantic/type_checker_ownership_let.c \
  src/semantic/type_checker_projection_path.c \
  src/semantic/type_checker_world_helpers.c \
  src/semantic/type_checker_zone_decl_authority.c
do
  grep -q 'semantic_type_resolution_lookup_type_ref_or_materialize' "$owner" || {
    echo "[type-resolution-resolver-inventory] semantic resolver owner lost metadata-first type-ref helper: $owner" >&2
    exit 1
  }
done

direct_materializer_users="$(
  grep -RIn 'semantic_type_resolution_lookup_or_materialize(ctx' src/semantic \
    | grep -Ev 'src/semantic/type_checker_resolution_metadata\.c' \
    || true
)"
if [ -n "$direct_materializer_users" ]; then
  echo "[type-resolution-resolver-inventory] semantic owner bypassed metadata-first type-ref helper:" >&2
  echo "$direct_materializer_users" >&2
  exit 1
fi

direct_metadata_materializer_count="$(
  grep -c 'semantic_type_resolution_lookup_or_materialize(ctx' \
    src/semantic/type_checker_resolution_metadata.c || true
)"
if [ "$direct_metadata_materializer_count" != "1" ]; then
  echo "[type-resolution-resolver-inventory] central metadata owner must keep exactly one type-ref helper materializer fallback (found $direct_metadata_materializer_count)" >&2
  exit 1
fi

grep -q 'metadata_type = resolve_named_type_from_metadata(name, ctx, site);' \
  src/semantic/type_checker_resolution_helpers.c || {
  echo "[type-resolution-resolver-inventory] resolve_named_type is no longer metadata-first" >&2
  exit 1
}

grep -q 'semantic_type_resolution_metadata_named_builtin_or_shell_singleton' \
  src/semantic/type_checker_resolution_helpers.c || {
  echo "[type-resolution-resolver-inventory] resolve_named_type no longer delegates builtin/shell lookup to metadata owner" >&2
  exit 1
}

if grep -q 'strcmp(name, "' src/semantic/type_checker_resolution_helpers.c; then
  echo "[type-resolution-resolver-inventory] resolve_named_type reintroduced local builtin/shell name table" >&2
  exit 1
fi

if grep -q 'resolve_named_type_from_metadata\|semantic_type_resolution_lookup_metadata_name_or_alias(ctx' \
  src/semantic/type_checker_resolution_helpers.h; then
  echo "[type-resolution-resolver-inventory] type_checker_resolution_helpers.h reintroduced implementation body" >&2
  exit 1
fi

materializer_recorders="$(
  grep -RIn 'semantic_type_resolution_record_materializer_fallback' src/semantic \
    | grep -v 'type_checker_internal.h' \
    | grep -v 'type_checker_resolution_metadata_fallback.c' \
    | grep -v 'type_checker_resolution_metadata.c' || true
)"
if [ -n "$materializer_recorders" ]; then
  echo "[type-resolution-resolver-inventory] materializer fallback recorder escaped central owner:" >&2
  printf '%s\n' "$materializer_recorders" >&2
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
  'strcmp(name, "Channel") == 0' \
  'strcmp(name, "Future") == 0' \
  'strcmp(name, "DeviceSlot") == 0' \
  'strcmp(name, "Slot") == 0' \
  'strcmp(name, "SecureSlot") == 0' \
  'type_node->type == AST_EVENT_HANDLER_TYPE' \
  'type_create_function(param_types, param_count, return_type)' \
  'type_node->data.type.tuple_elements != NULL' \
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

grep -q 'semantic_type_resolution_metadata_stable_builtin_shell_arity(' \
  src/semantic/type_checker_resolution_metadata_fallback.c || {
  echo "[type-resolution-resolver-inventory] metadata fallback owner stopped using centralized stable shell arity" >&2
  exit 1
}

for needle in \
  'case AST_CHANNEL_TYPE:' \
  'case AST_FUTURE_TYPE:' \
  'case AST_EVENT_HANDLER_TYPE:' \
  'semantic_type_resolution_try_record_stable_constructed_type(ctx, type_node)' \
  'type_node->data.type.tuple_elements != NULL'; do
  grep -q "$needle" src/semantic/type_checker_resolution_graph_collect.c || {
    echo "[type-resolution-resolver-inventory] graph collect materializer coverage missing: $needle" >&2
    exit 1
  }
done

echo "[type-resolution-resolver-inventory] direct resolver and fallback seam inventory are gated (fallback seams=$fallback_sites cap=0)"

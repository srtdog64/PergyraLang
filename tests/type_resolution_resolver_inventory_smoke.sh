#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

bad_direct="$(mktemp)"
bad_fallback="$(mktemp)"
trap 'rm -f "$bad_direct" "$bad_fallback"' EXIT

grep -RIn "resolve_type_node(" src/semantic | while IFS=: read -r path line text; do
  case "$path" in
    src/semantic/type_checker.h|src/semantic/type_checker_resolve.inc|src/semantic/type_checker_resolution_metadata.c)
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
  echo "Move new calls behind an owner-local resolver seam or explicitly document the legacy fallback allowlist." >&2
  exit 1
fi

grep -RIn 'semantic_type_resolution_resolve_or_fallback' src/semantic \
  | grep -v 'type_checker_internal.h' \
  | while IFS=: read -r path line text; do
  case "$path" in
    src/semantic/type_checker.c|\
    src/semantic/type_checker_ability_decl.c|\
    src/semantic/type_checker_ability_where.c|\
    src/semantic/type_checker_builtins_projection.c|\
    src/semantic/type_checker_builtins_query_domain.inc|\
    src/semantic/type_checker_class_decl.c|\
    src/semantic/type_checker_decls_domain_helpers.c|\
    src/semantic/type_checker_event.c|\
    src/semantic/type_checker_expr.inc|\
    src/semantic/type_checker_flow.c|\
    src/semantic/type_checker_generic_contracts.inc|\
    src/semantic/type_checker_generic_support.inc|\
    src/semantic/type_checker_generic_validation.c|\
    src/semantic/type_checker_helpers_effects.inc|\
    src/semantic/type_checker_helpers_host.inc|\
    src/semantic/type_checker_helpers_late.c|\
    src/semantic/type_checker_helpers_resolution.inc|\
    src/semantic/type_checker_intent_decl.c|\
    src/semantic/type_checker_intent_helpers.c|\
    src/semantic/type_checker_operator_expr.inc|\
    src/semantic/type_checker_ownership_destructure.c|\
    src/semantic/type_checker_ownership_let.c|\
    src/semantic/type_checker_party_decl.c|\
    src/semantic/type_checker_program.c|\
    src/semantic/type_checker_program.inc|\
    src/semantic/type_checker_resolution_metadata.c|\
    src/semantic/type_checker_resolution_stage.c|\
    src/semantic/type_checker_role_decl.c|\
    src/semantic/type_checker_roster_decl.c|\
    src/semantic/type_checker_world_decl.c|\
    src/semantic/type_checker_zone_decl.c)
      continue
      ;;
  esac
  printf '%s:%s: %s\n' "$path" "$line" "$text" >>"$bad_fallback"
done

if [ -s "$bad_fallback" ]; then
  echo "[type-resolution-resolver-inventory] unclassified metadata-first fallback seam(s):" >&2
  cat "$bad_fallback" >&2
  echo "Classify new fallback users as graph-backed owner seams or explicit legacy seams before landing." >&2
  exit 1
fi

fallback_sites="$(
  grep -RIn 'semantic_type_resolution_resolve_or_fallback' src/semantic \
    | grep -v 'type_checker_internal.h' \
    | wc -l
)"
if [ "$fallback_sites" -gt 31 ]; then
  echo "[type-resolution-resolver-inventory] metadata-first fallback seam inventory grew: $fallback_sites > 31" >&2
  echo "Shrink or explicitly justify the fallback owner allowlist before adding new seams." >&2
  exit 1
fi

grep -q 'metadata_type = semantic_type_resolution_lookup_resolved_type(ctx, node);' \
  src/semantic/type_checker_resolve.inc || {
  echo "[type-resolution-resolver-inventory] resolve_type_node is no longer metadata-first" >&2
  exit 1
}

for needle in \
  'strcmp(name, "Channel") == 0' \
  'strcmp(name, "Future") == 0' \
  'strcmp(name, "DeviceSlot") == 0' \
  'strcmp(type_node->data.type.name, "Slot") == 0' \
  'strcmp(type_node->data.type.name, "SecureSlot") == 0' \
  'type_node->type == AST_EVENT_HANDLER_TYPE' \
  'type_create_function(param_types, param_count, return_type)' \
  'type_node->data.type.tuple_elements != NULL' \
  'type_create_tuple(elements, element_count)' \
  'TYPE_LONG' \
  'TYPE_BOOL'; do
  grep -q "$needle" src/semantic/type_checker_resolution_metadata.c || {
    echo "[type-resolution-resolver-inventory] metadata materializer missing: $needle" >&2
    exit 1
  }
done

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

echo "[type-resolution-resolver-inventory] direct resolver and fallback seam inventory are gated (fallback seams=$fallback_sites cap=31; lower is better)"

#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

bad_direct="$(mktemp)"
bad_fallback="$(mktemp)"
fallback_matches="$(mktemp)"
trap 'rm -f "$bad_direct" "$bad_fallback" "$fallback_matches"' EXIT

grep -RIn "resolve_type_node(" src/semantic | while IFS=: read -r path line text; do
  case "$path" in
    src/semantic/type_checker.h|src/semantic/type_checker_resolve.h|src/semantic/type_checker_resolution_metadata.c)
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
  echo "Classify new fallback users as graph-backed owner seams or explicit legacy seams before landing." >&2
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

grep -q 'metadata_type = semantic_type_resolution_lookup_resolved_type(ctx, node);' \
  src/semantic/type_checker_resolve.h || {
  echo "[type-resolution-resolver-inventory] resolve_type_node is no longer metadata-first" >&2
  exit 1
}

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
  grep -RIn 'return resolve_type_node(type_node, ctx);' src/semantic/type_checker_resolution_metadata.c \
    | wc -l
)"
if [ "$metadata_fallback_escape_count" -ne 1 ]; then
  echo "[type-resolution-resolver-inventory] central metadata fallback escape hatch count changed: $metadata_fallback_escape_count" >&2
  exit 1
fi

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
  grep -q "$needle" \
    src/semantic/type_checker_resolution_metadata.c \
    src/semantic/type_checker_resolution_metadata_constructed.c || {
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

echo "[type-resolution-resolver-inventory] direct resolver and fallback seam inventory are gated (fallback seams=$fallback_sites cap=0)"

#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

bad="$(mktemp)"
trap 'rm -f "$bad"' EXIT

grep -RIn "resolve_type_node(" src/semantic | while IFS=: read -r path line text; do
  case "$path" in
    src/semantic/type_checker.h|src/semantic/type_checker_resolve.inc)
      continue
      ;;
  esac

  trimmed="$(printf '%s' "$text" | sed 's/^[[:space:]]*//')"
  case "$trimmed" in
    *'resolve_type_node("T")'*)
      continue
      ;;
    'return resolve_type_node('*)
      continue
      ;;
  esac

  case "$path:$trimmed" in
    'src/semantic/type_checker_program.c:resolved = resolve_type_node(type_node, ctx);'|\
    'src/semantic/type_checker_resolution_stage.c:resolved = resolve_type_node(type_node, ctx);'|\
    'src/semantic/type_checker_resolution_stage.c:: resolve_type_node(decl->data.type_alias.target_type, ctx);'|\
    'src/semantic/type_checker_helpers_resolution.inc:Type *resolved = resolve_type_node(alias_decl->data.type_alias.target_type, ctx);')
      continue
      ;;
  esac

  printf '%s:%s: %s\n' "$path" "$line" "$trimmed" >>"$bad"
done

if [ -s "$bad" ]; then
  echo "[type-resolution-resolver-inventory] unexpected direct resolver call(s):" >&2
  cat "$bad" >&2
  echo "Move new calls behind an owner-local resolver seam or explicitly document the legacy fallback allowlist." >&2
  exit 1
fi

echo "[type-resolution-resolver-inventory] direct resolver inventory is seam-gated"

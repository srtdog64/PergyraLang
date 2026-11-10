#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
unit="${PGY_MODULE_SOURCE_UNIT:-$ROOT_DIR/bin/module_source_snapshot_unit}"
unit="$(pgy_select_optional_exe_binary "$unit")"
pgy_require_runnable_binary_here module-source-snapshot "$unit"
pgy_prepend_windows_runtime_paths
mkdir -p "$ROOT_DIR/.tmp"
test_dir="$(mktemp -d "$ROOT_DIR/.tmp/module-source-snapshot.XXXXXX")"
"$unit" "$(pgy_path_for_compiler "$unit" "$test_dir")"

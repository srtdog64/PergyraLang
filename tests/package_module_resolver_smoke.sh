#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"

if [[ ! -x "$PGY" ]]; then
    echo "[package-module] missing compiler binary: $PGY" >&2
    exit 1
fi

WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_pkg_module.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

CONTRACT_DOC="$ROOT_DIR/docs/109_package_module_resolver_contract.md"
if [[ ! -f "$CONTRACT_DOC" ]]; then
    echo "[package-module] missing resolver contract doc: $CONTRACT_DOC" >&2
    exit 1
fi
for required in \
    "Package And Module Resolver Beta Contract" \
    "beta-freeze-source-of-truth" \
    "import \"relative/path.pgy\";" \
    "Import paths are resolved relative to the importing file" \
    "Only manifest scaffolding is beta-stable" \
    "pgy init <name>" \
    "pgy install" \
    "Dependency version solving" \
    "supply-chain integrity" \
    "JSON diagnostics for module-load failures"; do
    if ! grep -Fq "$required" "$CONTRACT_DOC"; then
        echo "[package-module] contract doc missing: $required" >&2
        exit 1
    fi
done

INIT_DIR="$WORK_DIR/init_case"
mkdir -p "$INIT_DIR"
(
    cd "$INIT_DIR"
    "$PGY" init sample-app >init.out 2>init.err
)
grep -Fq "pgy init: created pgy.toml for 'sample-app'" "$INIT_DIR/init.out"
grep -Fq "pgy init: created main.pgy" "$INIT_DIR/init.out"
test ! -s "$INIT_DIR/init.err"
grep -Fq '[package]' "$INIT_DIR/pgy.toml"
grep -Fq 'name = "sample-app"' "$INIT_DIR/pgy.toml"
grep -Fq 'version = "0.1.0"' "$INIT_DIR/pgy.toml"
grep -Fq 'pergyra = "1.0"' "$INIT_DIR/pgy.toml"
grep -Fq 'entry = "main.pgy"' "$INIT_DIR/pgy.toml"
grep -Fq '[dependencies]' "$INIT_DIR/pgy.toml"
grep -Fq '[dev-dependencies]' "$INIT_DIR/pgy.toml"
init_run_output="$(cd "$INIT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$INIT_DIR/main.pgy")" --backend=c --run 2>&1)"
grep -Fq "Hello, sample-app!" <<<"$init_run_output"

if install_output="$("$PGY" install example 2>&1)"; then
    echo "[package-module] pgy install unexpectedly succeeded" >&2
    exit 1
fi
grep -Fq "pgy install: package resolution and registry install are out-of-beta" \
    <<<"$install_output"

MISSING_DIR="$WORK_DIR/missing_import"
mkdir -p "$MISSING_DIR"
cat > "$MISSING_DIR/main.pgy" <<'EOF'
import "missing.pgy";
func Main() -> Void { Log(1); }
EOF
missing_json="$MISSING_DIR/missing.err"
if "$PGY" "$(pgy_path_for_compiler "$PGY" "$MISSING_DIR/main.pgy")" --backend=c --error-format=json 2>"$missing_json"; then
    echo "[package-module] missing import unexpectedly succeeded" >&2
    exit 1
fi

CYCLE_DIR="$WORK_DIR/cycle"
mkdir -p "$CYCLE_DIR"
cat > "$CYCLE_DIR/a.pgy" <<'EOF'
import "b.pgy";
func A() -> Int { return 1; }
EOF
cat > "$CYCLE_DIR/b.pgy" <<'EOF'
import "a.pgy";
func B() -> Int { return 2; }
EOF
cat > "$CYCLE_DIR/main.pgy" <<'EOF'
import "a.pgy";
func Main() -> Void { Log(1); }
EOF
cycle_json="$CYCLE_DIR/cycle.err"
if "$PGY" "$(pgy_path_for_compiler "$PGY" "$CYCLE_DIR/main.pgy")" --backend=c --error-format=json 2>"$cycle_json"; then
    echo "[package-module] circular import unexpectedly succeeded" >&2
    exit 1
fi

PY_BIN=""
if command -v python3 >/dev/null 2>&1; then
    PY_BIN="$(command -v python3)"
elif command -v python >/dev/null 2>&1; then
    PY_BIN="$(command -v python)"
fi

if [[ -n "$PY_BIN" ]]; then
    "$PY_BIN" - "$missing_json" "$cycle_json" <<'PY'
import json
import sys

missing_path, cycle_path = sys.argv[1], sys.argv[2]
missing = json.loads(open(missing_path, encoding="utf-8").read())
cycle = json.loads(open(cycle_path, encoding="utf-8").read())

if not (isinstance(missing, list)
        and len(missing) == 1
        and missing[0].get("stage") == "module_load"
        and "cannot open" in missing[0].get("message", "")):
    raise SystemExit(f"unexpected missing-import json: {missing}")

if not (isinstance(cycle, list)
        and len(cycle) == 1
        and cycle[0].get("stage") == "module_load"
        and "circular import detected" in cycle[0].get("message", "")):
    raise SystemExit(f"unexpected circular-import json: {cycle}")
PY
fi

echo "[package-module] resolver/package beta contract ok"

#!/usr/bin/env bash
set -euo pipefail

# Subject of this gate:
#   the package module resolver changed.
# That is a fact about the native pipeline, so the gate compiles
# in-process instead of delegating to the installed self-host driver.
# Delegated, a self-host coverage gap would read as a regression in
# the subject above. Declared per harness because the compiler is
# reached through make and nested scripts, and the variable is the
# same declared opt-out as --native-pipeline -- never a fallback.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

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
    "Seashell Manifest Surface" \
    "pgy.seashell.v1" \
    "toml-subset" \
    "pgy.toml stays TOML" \
    "Seashell is not a second executable" \
    "Backends consume owner facts, not raw manifest text" \
    "Unsupported TOML constructs fail closed" \
    "Non-empty" \
    "pgy.lock" \
    "local manifest-driven package commands are stable" \
    "pgy init <name>" \
    "pgy check" \
    "pgy build" \
    "pgy run" \
    "pgy test" \
    "pgy package" \
    "pgy install" \
    "pgy publish" \
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
grep -Fq "pgy package: wrote pgy.lock for 'sample-app'" "$INIT_DIR/init.out"
test ! -s "$INIT_DIR/init.err"
grep -Fq '[package]' "$INIT_DIR/pgy.toml"
grep -Fq '[seashell]' "$INIT_DIR/pgy.toml"
grep -Fq 'schema = "pgy.seashell.v1"' "$INIT_DIR/pgy.toml"
grep -Fq 'format = "toml-subset"' "$INIT_DIR/pgy.toml"
grep -Fq 'name = "sample-app"' "$INIT_DIR/pgy.toml"
grep -Fq 'version = "0.1.0"' "$INIT_DIR/pgy.toml"
grep -Fq 'pergyra = "1.0"' "$INIT_DIR/pgy.toml"
grep -Fq 'edition = "2026"' "$INIT_DIR/pgy.toml"
grep -Fq '[targets.app]' "$INIT_DIR/pgy.toml"
grep -Fq 'main = "main.pgy"' "$INIT_DIR/pgy.toml"
grep -Fq '[effects]' "$INIT_DIR/pgy.toml"
grep -Fq '[authority]' "$INIT_DIR/pgy.toml"
grep -Fq '[capabilities]' "$INIT_DIR/pgy.toml"
grep -Fq '[build]' "$INIT_DIR/pgy.toml"
grep -Fq 'allow = []' "$INIT_DIR/pgy.toml"
grep -Fq 'deny = []' "$INIT_DIR/pgy.toml"
grep -Fq 'backend = "c"' "$INIT_DIR/pgy.toml"
grep -Fq '[dependencies]' "$INIT_DIR/pgy.toml"
grep -Fq '[dev-dependencies]' "$INIT_DIR/pgy.toml"
grep -Fq '[[package]]' "$INIT_DIR/pgy.lock"
grep -Fq '[seashell]' "$INIT_DIR/pgy.lock"
grep -Fq 'schema = "pgy.seashell.lock.v1"' "$INIT_DIR/pgy.lock"
grep -Fq 'manifest_schema = "pgy.seashell.v1"' "$INIT_DIR/pgy.lock"
grep -Fq 'source = "path:."' "$INIT_DIR/pgy.lock"
init_run_output="$(cd "$INIT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$INIT_DIR/main.pgy")" --backend=c --run 2>&1)"
grep -Fq "Hello, sample-app!" <<<"$init_run_output"
pkg_check_output="$(cd "$INIT_DIR" && "$PGY" check 2>&1)"
grep -Fq "pgy check: main.pgy ok" <<<"$pkg_check_output"
pkg_build_output="$(cd "$INIT_DIR" && "$PGY" build 2>&1)"
grep -Fq "pgy build: main.pgy ok" <<<"$pkg_build_output"
pkg_run_output="$(cd "$INIT_DIR" && "$PGY" run 2>&1)"
grep -Fq "Hello, sample-app!" <<<"$pkg_run_output"
grep -Fq "pgy run: main.pgy ok" <<<"$pkg_run_output"
pkg_test_output="$(cd "$INIT_DIR" && "$PGY" test 2>&1)"
grep -Fq "Hello, sample-app!" <<<"$pkg_test_output"
grep -Fq "pgy test: main.pgy ok" <<<"$pkg_test_output"
pkg_fmt_output="$(cd "$INIT_DIR" && "$PGY" fmt --check 2>&1)"
test -z "$pkg_fmt_output"
pkg_lint_output="$(cd "$INIT_DIR" && "$PGY" lint 2>&1)"
grep -Fq "pgy lint: main.pgy ok" <<<"$pkg_lint_output"
pkg_prove_output="$(cd "$INIT_DIR" && "$PGY" prove 2>&1)"
grep -Fq "pgy prove: main.pgy ok" <<<"$pkg_prove_output"
grep -Fq "pgy prove: package evidence preflight ok (not a theorem)" <<<"$pkg_prove_output"
pkg_package_output="$(cd "$INIT_DIR" && "$PGY" package 2>&1)"
grep -Fq "pgy package-check: main.pgy ok" <<<"$pkg_package_output"
grep -Fq "pgy package: wrote pgy.lock for 'sample-app'" <<<"$pkg_package_output"

if install_output="$("$PGY" install example 2>&1)"; then
    echo "[package-module] pgy install unexpectedly succeeded" >&2
    exit 1
fi
grep -Fq "pgy install: dependency version solving and registry install are out-of-beta" \
    <<<"$install_output"

BAD_NAME_DIR="$WORK_DIR/bad_name_case"
mkdir -p "$BAD_NAME_DIR"
if bad_name_output="$(cd "$BAD_NAME_DIR" && "$PGY" init 'bad"name' 2>&1)"; then
    echo "[package-module] invalid package name unexpectedly succeeded" >&2
    exit 1
fi
grep -Fq "package name must use only letters" <<<"$bad_name_output"
test ! -f "$BAD_NAME_DIR/pgy.toml"

if publish_output="$(cd "$INIT_DIR" && "$PGY" publish 2>&1)"; then
    echo "[package-module] pgy publish unexpectedly succeeded" >&2
    exit 1
fi
grep -Fq "pgy publish: registry publishing is out-of-beta" <<<"$publish_output"

DEP_DIR="$WORK_DIR/dependency_case"
mkdir -p "$DEP_DIR"
(
    cd "$DEP_DIR"
    "$PGY" init dep-app >/dev/null
    cat > pgy.toml <<'EOF'
[seashell]
schema = "pgy.seashell.v1"
format = "toml-subset"

[package]
name = "dep-app"
version = "0.1.0"

[targets.app]
main = "main.pgy"

[build]
backend = "c"
deterministic = true

[dependencies]
example = "0.1.0"
EOF
)
if dep_output="$(cd "$DEP_DIR" && "$PGY" check 2>&1)"; then
    echo "[package-module] dependency entry unexpectedly succeeded" >&2
    exit 1
fi
grep -Fq "dependency version solving is out-of-beta" <<<"$dep_output"

SCHEMA_DIR="$WORK_DIR/schema_case"
mkdir -p "$SCHEMA_DIR"
(
    cd "$SCHEMA_DIR"
    "$PGY" init schema-app >/dev/null
    sed -i.bak '/schema = "pgy.seashell.v1"/d' pgy.toml
)
if schema_output="$(cd "$SCHEMA_DIR" && "$PGY" check 2>&1)"; then
    echo "[package-module] missing Seashell schema unexpectedly succeeded" >&2
    exit 1
fi
grep -Fq 'requires [seashell] schema = "pgy.seashell.v1"' <<<"$schema_output"

FORMAT_DIR="$WORK_DIR/format_case"
mkdir -p "$FORMAT_DIR"
(
    cd "$FORMAT_DIR"
    "$PGY" init format-app >/dev/null
    sed -i.bak 's/format = "toml-subset"/format = "yaml"/' pgy.toml
)
if format_output="$(cd "$FORMAT_DIR" && "$PGY" check 2>&1)"; then
    echo "[package-module] wrong Seashell format unexpectedly succeeded" >&2
    exit 1
fi
grep -Fq 'requires [seashell] format = "toml-subset"' <<<"$format_output"

BACKEND_ARRAY_DIR="$WORK_DIR/backend_array_case"
mkdir -p "$BACKEND_ARRAY_DIR"
(
    cd "$BACKEND_ARRAY_DIR"
    "$PGY" init backend-array-app >/dev/null
    sed -i.bak 's/backend = "c"/backend = ["c", "llvm"]/' pgy.toml
)
if backend_array_output="$(cd "$BACKEND_ARRAY_DIR" && "$PGY" check 2>&1)"; then
    echo "[package-module] backend array unexpectedly succeeded" >&2
    exit 1
fi
grep -Fq "[build].backend must be a scalar string" <<<"$backend_array_output"

CAPABILITY_DIR="$WORK_DIR/capability_case"
mkdir -p "$CAPABILITY_DIR"
(
    cd "$CAPABILITY_DIR"
    "$PGY" init capability-app >/dev/null
    awk '{ if ($0 == "allow = []") print "allow = [\"log\"]"; else print $0 }' \
        pgy.toml > pgy.toml.tmp
    mv pgy.toml.tmp pgy.toml
)
if capability_output="$(cd "$CAPABILITY_DIR" && "$PGY" check 2>&1)"; then
    echo "[package-module] non-empty capability declaration unexpectedly succeeded" >&2
    exit 1
fi
grep -Fq "non-empty capability declarations require a capability verifier owner" \
    <<<"$capability_output"

ENTRY_ESCAPE_DIR="$WORK_DIR/entry_escape_case"
mkdir -p "$ENTRY_ESCAPE_DIR"
(
    cd "$ENTRY_ESCAPE_DIR"
    "$PGY" init escape-app >/dev/null
    awk 'BEGIN { done = 0 }
        { if (!done && $0 == "main = \"main.pgy\"") {
              print "main = \"../main.pgy\""; done = 1
          } else { print $0 } }' pgy.toml > pgy.toml.tmp
    mv pgy.toml.tmp pgy.toml
)
if entry_escape_output="$(cd "$ENTRY_ESCAPE_DIR" && "$PGY" check 2>&1)"; then
    echo "[package-module] escaping target path unexpectedly succeeded" >&2
    exit 1
fi
grep -Fq "target entry paths must stay inside the package" <<<"$entry_escape_output"

LOCK_DRIFT_DIR="$WORK_DIR/lock_drift_case"
mkdir -p "$LOCK_DRIFT_DIR"
(
    cd "$LOCK_DRIFT_DIR"
    "$PGY" init lock-app >/dev/null
    sed -i.bak 's/version = "0.1.0"/version = "0.2.0"/' pgy.toml
)
if lock_drift_output="$(cd "$LOCK_DRIFT_DIR" && "$PGY" check 2>&1)"; then
    echo "[package-module] lock drift unexpectedly succeeded" >&2
    exit 1
fi
grep -Fq "pgy.lock drift detected" <<<"$lock_drift_output"
lock_refresh_output="$(cd "$LOCK_DRIFT_DIR" && "$PGY" package 2>&1)"
grep -Fq "pgy package: wrote pgy.lock for 'lock-app'" <<<"$lock_refresh_output"
lock_check_output="$(cd "$LOCK_DRIFT_DIR" && "$PGY" check 2>&1)"
grep -Fq "pgy check: main.pgy ok" <<<"$lock_check_output"

NEW_DIR="$WORK_DIR/new_case"
"$PGY" new "$(pgy_path_for_compiler "$PGY" "$NEW_DIR")" >"$WORK_DIR/new.out" 2>"$WORK_DIR/new.err"
test ! -s "$WORK_DIR/new.err"
grep -Fq "pgy: scaffolded project" "$WORK_DIR/new.out"
grep -Fq '[targets.app]' "$NEW_DIR/pgy.toml"
grep -Fq 'schema = "pgy.seashell.v1"' "$NEW_DIR/pgy.toml"
grep -Fq 'format = "toml-subset"' "$NEW_DIR/pgy.toml"
grep -Fq 'backend = "c"' "$NEW_DIR/pgy.toml"
grep -Fq '[[package]]' "$NEW_DIR/pgy.lock"
grep -Fq 'schema = "pgy.seashell.lock.v1"' "$NEW_DIR/pgy.lock"

HYPHEN_DIR="$WORK_DIR/hyphen-project"
"$PGY" new "$(pgy_path_for_compiler "$PGY" "$HYPHEN_DIR")" >"$WORK_DIR/hyphen.out" 2>"$WORK_DIR/hyphen.err"
test ! -s "$WORK_DIR/hyphen.err"
grep -Fq 'name = "hyphen-project"' "$HYPHEN_DIR/pgy.toml"
grep -Fq "world hyphen_projectWorld" "$HYPHEN_DIR/world.pgy"
hyphen_check_output="$(cd "$HYPHEN_DIR" && "$PGY" check 2>&1)"
grep -Fq "pgy check: main.pgy ok" <<<"$hyphen_check_output"

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

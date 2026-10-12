#!/usr/bin/env bash
# AgentHub demo. Builds one agent turn for tenant acme, then rebuilds it with one common
# mistake at a time and shows Pergyra's verdict next to TypeScript's. The mistakes are
# derived from agenthub.pgy at run time, so they cannot drift from the correct program.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
cd "$ROOT_DIR"
source tests/pgy_binary_path_helpers.sh
pgy_prepend_windows_runtime_paths

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-bin/pgy}")"
DEMO=tests/counterexample_corpus/demo_agenthub
SOURCE="$DEMO/agenthub.pgy"
[[ -x "$PGY" ]] || { echo "[agenthub-demo] missing compiler: $PGY (set PGY_BIN)"; exit 1; }
mkdir -p .tmp
WORK="$(mktemp -d .tmp/agenthub-demo.XXXXXX)"
failures=0
say() { printf '%s\n' "$*"; }
bad() { say "FAIL: $*"; failures=$((failures + 1)); }

first_error() {
    python3 - "$1" <<'PYEOF'
import json, sys
text = open(sys.argv[1], encoding="utf-8", errors="replace").read().strip()
diags = json.loads(text.splitlines()[0]) if text.startswith("[") else []
errors = [d for d in diags if d.get("severity") == "error"]
if errors:
    print(errors[0].get("code", "-") + " | " + errors[0]["message"].split("\n")[0])
PYEOF
}

say "== the correct agent turn"
"$PGY" "$SOURCE" --native-pipeline --backend=c -o "$WORK/agenthub.exe" >"$WORK/build.log" 2>&1 ||
    { bad "agenthub.pgy does not build ($WORK/build.log)"; exit 1; }
timeout 60 "$WORK/agenthub.exe" | tr -d '\r' | tee "$WORK/agenthub.out"
cmp -s "$DEMO/agenthub.expected" "$WORK/agenthub.out" || bad "output differs from agenthub.expected"

# name | sed program | expected code or message (the first error must contain it)
mistakes=(
    "wrong_tenant|s/intent ChargeTurn(tenant: AcmeTenant,/intent ChargeTurn(tenant: GlobexTenant,/;s/        where: AcmeTenant;/        where: GlobexTenant;/;s/ChargeTurn(acme, books, acme.agent, acme.owner,/ChargeTurn(globex, books, acme.agent, globex.owner,/|resolves to non-authority slot 'agent' in zone 'GlobexTenant'"
    "undeclared_capability|s/^    Print(\"reply to \"/    let started: Long = Now();\n    Print(\"reply to \"/|missing declared capabilities: clock"
    "orphan_tool_call|s/    let hits: Int = await search;/    let hits: Int = 0;/|PGY_SEM_TASK_LIFECYCLE"
    "missing_compensation|/        compensate: owner.Refund(cost);/d|no compensation under 'rollback: full'"
    "execute_before_plan|/    run.Plan();/d|Lifecycle violation: 'Execute' on 'run' is not permitted in state 'Pending'"
)

ts_ready=0
if command -v tsc >/dev/null 2>&1 && command -v node >/dev/null 2>&1; then
    if tsc --strict --noEmitOnError --target es2022 --module nodenext --outDir "$WORK/ts" \
            "$DEMO/agenthub.ts" >"$WORK/ts.build" 2>&1; then
        ts_ready=1
    else
        bad "agenthub.ts does not pass tsc --strict ($WORK/ts.build)"
    fi
fi

for row in "${mistakes[@]}"; do
    IFS='|' read -r name program expect <<<"$row"
    say ""
    say "== mistake: $name"
    sed "$program" "$SOURCE" >"$WORK/$name.pgy"
    diff "$SOURCE" "$WORK/$name.pgy" | grep '^[<>]' | sed 's/^/   /' || true
    rc=0
    "$PGY" --native-pipeline --mir-json --error-format=json "$WORK/$name.pgy" >"$WORK/$name.diag" 2>&1 || rc=$?
    verdict="$(first_error "$WORK/$name.diag")"
    say "   Pergyra:    refused before the program exists -> $verdict"
    if [[ "$rc" -ne 1 || "$verdict" != *"$expect"* ]]; then bad "$name was not refused with '$expect'"; fi
    if [[ "$ts_ready" -eq 1 ]]; then
        timeout 60 node "$WORK/ts/agenthub.js" "$name" | tr -d '\r' >"$WORK/$name.ts.out"
        line="$(grep '^MISTAKE ' "$WORK/$name.ts.out" | head -1 || true)"
        say "   TypeScript: tsc --strict accepted it -> ${line:-<no mistake line>}"
        [[ -n "$line" ]] || bad "$name: TypeScript version did not show the mistake"
    else
        say "   TypeScript: SKIP (tsc or node missing)"
    fi
done

say ""
say "== at run time: the host withholds io_write"
rc=0
PGY_CAP_GRANT=clock timeout 60 "$WORK/agenthub.exe" >"$WORK/grant.out" 2>&1 || rc=$?
say "   Pergyra:    $(grep -o 'class=[a-z-]*' "$WORK/grant.out" | head -1), exit $rc, before any charge"
grep -q 'class=capability-denied' "$WORK/grant.out" || bad "missing capability-denied"
grep -q '^turn 1' "$WORK/grant.out" && bad "a charge ran without the grant"

say ""
say "== at run time: the host allows no tool tasks"
rc=0
PGY_BUDGET_SPAWN_COUNT=0 timeout 60 "$WORK/agenthub.exe" >"$WORK/spawn.out" 2>&1 || rc=$?
say "   Pergyra:    $(grep -o 'class=[a-z-]*' "$WORK/spawn.out" | head -1), exit $rc"
grep -q 'class=budget-exceeded' "$WORK/spawn.out" || bad "missing budget-exceeded"

say ""
if [[ "$failures" -ne 0 ]]; then
    say "[agenthub-demo] $failures failure(s); evidence: $WORK"
    exit 1
fi
say "[agenthub-demo] PASS: 5 mistakes refused at compile time, 2 host limits enforced at run time; evidence: $WORK"

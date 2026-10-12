#!/usr/bin/env bash
# Counterexample corpus gate (manual; see tests/counterexample_corpus/README.md).
#
# Each manifest row fixes what Pergyra does with one bug class from
# docs/198_market_safety_positioning.md. The bug program must be refused at
# compile time (REJECT_COMPILE), stop fail-closed at run time (TRAP_RUNTIME),
# keep state correct at run time (RUNTIME_SAFE), or still be accepted
# (ACCEPTED_GAP, a recorded gap: if Pergyra starts refusing it, the row must be
# updated). The fixed program must build and print its expected output. The
# TypeScript, C# and Python versions run when their toolchain is present and
# must show the bug; a missing toolchain is reported as SKIP, never as a pass.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
cd "$ROOT_DIR"
source tests/pgy_binary_path_helpers.sh
pgy_prepend_windows_runtime_paths

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-bin/pgy}")"
CORPUS=tests/counterexample_corpus
MANIFEST="$CORPUS/manifest.tsv"
OTHERS="${PGY_CORPUS_OTHERS-1}"

fail_count=0
note() { printf '[counterexample-corpus] %s\n' "$*"; }
bad() { note "FAIL $*"; fail_count=$((fail_count + 1)); }

[[ -x "$PGY" ]] || { note "missing compiler: $PGY (set PGY_BIN)"; exit 1; }
[[ -f "$MANIFEST" ]] || { note "missing manifest: $MANIFEST"; exit 1; }
grep -qx '# schema pgy.counterexample.v1' "$MANIFEST" || { note "manifest schema line missing"; exit 1; }

mkdir -p .tmp
WORK="$(mktemp -d .tmp/counterexample-corpus.XXXXXX)"

# Run a command with the optional `KEY=VALUE[;KEY=VALUE]` environment of a row.
run_with_env() {
    local spec="$1"; shift
    if [[ "$spec" == "-" ]]; then
        timeout 60 "$@"
        return
    fi
    local -a assignments=()
    IFS=';' read -r -a assignments <<<"$spec"
    timeout 60 env "${assignments[@]}" "$@"
}

build() {
    local source="$1" out="$2" log="$3"
    "$PGY" "$source" --native-pipeline --backend=c -o "$out" >"$log" 2>&1
}

check_fixed() {
    local id="$1" dir="$2"
    local exe="$WORK/$id-fixed.exe"
    if ! build "$dir/pergyra_fixed.pgy" "$exe" "$WORK/$id-fixed.build"; then
        bad "$id fixed program does not build ($WORK/$id-fixed.build)"; return
    fi
    if ! timeout 60 "$exe" >"$WORK/$id-fixed.raw" 2>"$WORK/$id-fixed.err"; then
        bad "$id fixed program exited non-zero"; return
    fi
    tr -d '\r' <"$WORK/$id-fixed.raw" >"$WORK/$id-fixed.out"
    if ! cmp -s "$dir/pergyra_fixed.expected" "$WORK/$id-fixed.out"; then
        bad "$id fixed program output differs from pergyra_fixed.expected"
    fi
}

check_bug() {
    local id="$1" dir="$2" expect="$3" oracle="$4" env_spec="$5"
    local source="$dir/pergyra_bug.pgy" exe="$WORK/$id-bug.exe"
    case "$expect" in
        REJECT_COMPILE)
            local rc=0
            "$PGY" --native-pipeline --mir-json --error-format=json "$source" >"$WORK/$id-bug.diag" 2>&1 || rc=$?
            if [[ "$rc" -ne 1 ]]; then bad "$id bug program was not refused (rc=$rc)"; return; fi
            if grep -q 'pgy.mir.v1' "$WORK/$id-bug.diag"; then bad "$id refusal still emitted MIR"; return; fi
            case "$oracle" in
                code:*) grep -Fq "\"code\":\"${oracle#code:}\"" "$WORK/$id-bug.diag" ||
                    bad "$id refusal lacks ${oracle#code:}" ;;
                message:*) grep -Fq "${oracle#message:}" "$WORK/$id-bug.diag" ||
                    bad "$id refusal lacks message '${oracle#message:}'" ;;
                *) bad "$id REJECT_COMPILE oracle must be code: or message:" ;;
            esac
            ;;
        TRAP_RUNTIME)
            if ! build "$source" "$exe" "$WORK/$id-bug.build"; then bad "$id bug program does not build"; return; fi
            local rc=0
            run_with_env "$env_spec" "$exe" >"$WORK/$id-bug.run" 2>&1 || rc=$?
            if [[ "$rc" -eq 0 ]]; then bad "$id bug program was not stopped at run time"; return; fi
            [[ "$oracle" == class:* ]] || { bad "$id TRAP_RUNTIME oracle must be class:"; return; }
            grep -Fq "class=${oracle#class:}" "$WORK/$id-bug.run" || bad "$id run lacks class=${oracle#class:}"
            ;;
        RUNTIME_SAFE)
            if ! build "$source" "$exe" "$WORK/$id-bug.build"; then bad "$id bug program does not build"; return; fi
            if ! run_with_env "$env_spec" "$exe" >"$WORK/$id-bug.raw" 2>"$WORK/$id-bug.err"; then
                bad "$id bug program exited non-zero"; return
            fi
            tr -d '\r' <"$WORK/$id-bug.raw" >"$WORK/$id-bug.out"
            cmp -s "$dir/pergyra_bug.expected" "$WORK/$id-bug.out" ||
                bad "$id bug program output differs from pergyra_bug.expected"
            ;;
        ACCEPTED_GAP)
            # A gap stays a gap: the bug program must still build. If Pergyra now
            # refuses it, the class moved and this row has to say so.
            build "$source" "$exe" "$WORK/$id-bug.build" ||
                bad "$id gap row is now refused; update the manifest ($WORK/$id-bug.build)"
            ;;
        *) bad "$id unknown pergyra_expect '$expect'" ;;
    esac
}

have() { command -v "$1" >/dev/null 2>&1; }

check_others() {
    local id="$1" dir="$2" cs_observed="$3"
    local marker="BUG_MANIFESTED ${id%%_*}"
    if have tsc && have node; then
        local out="$WORK/ts-$id"
        if tsc --strict --noEmitOnError --target es2022 --module nodenext --outDir "$out" \
                "$dir/typescript.ts" >"$WORK/$id-ts.build" 2>&1 &&
            timeout 60 node "$out/typescript.js" >"$WORK/$id-ts.run" 2>&1 &&
            grep -Fqx "$marker" <(tr -d '\r' <"$WORK/$id-ts.run"); then
            ts_status=ok
        else
            bad "$id TypeScript version did not build cleanly and show the bug"; ts_status=fail
        fi
    else
        ts_status=SKIP
    fi
    if have dotnet; then
        local build_rc=0
        dotnet build --no-incremental "$dir/csharp.cs" >"$WORK/$id-cs.build" 2>&1 || build_rc=$?
        if [[ "$build_rc" -ne 0 ]]; then
            bad "$id C# version does not build"; cs_status=fail
        elif ! timeout 120 dotnet run "$dir/csharp.cs" >"$WORK/$id-cs.run" 2>&1 ||
            ! grep -Fqx "$marker" <(tr -d '\r' <"$WORK/$id-cs.run"); then
            bad "$id C# version did not show the bug"; cs_status=fail
        else
            cs_status=ok
            # The recorded compiler reaction must be the observed one.
            local warning
            warning="$(grep -oE 'warning CS[0-9]{4}' <<<"$cs_observed" | head -1 || true)"
            if [[ -n "$warning" ]]; then
                grep -Fq "${warning#warning }" "$WORK/$id-cs.build" || bad "$id C# build lacks recorded $warning"
            elif grep -Eq 'warning CS[0-9]{4}' "$WORK/$id-cs.build"; then
                bad "$id C# build has a warning the manifest does not record"
            fi
        fi
    else
        cs_status=SKIP
    fi
    if have python; then
        if timeout 60 python "$dir/python.py" >"$WORK/$id-py.run" 2>&1 &&
            grep -Fqx "$marker" <(tr -d '\r' <"$WORK/$id-py.run"); then
            py_status=ok
        else
            bad "$id Python version did not show the bug"; py_status=fail
        fi
    else
        py_status=SKIP
    fi
}

rows=0
declare -A by_expect=()
while IFS=$'\t' read -r id category bug scenario level expect oracle env_spec ts_obs cs_obs py_obs gap; do
    [[ -z "$id" || "$id" == \#* || "$id" == id ]] && continue
    rows=$((rows + 1))
    dir="$CORPUS/$id"
    for file in pergyra_bug.pgy pergyra_fixed.pgy pergyra_fixed.expected typescript.ts csharp.cs python.py; do
        [[ -f "$dir/$file" ]] || bad "$id missing $file"
    done
    case "$level" in COMPILE|RUNTIME|PARTIAL|NONE) ;; *) bad "$id unknown pergyra_level '$level'" ;; esac
    if [[ "$expect" == ACCEPTED_GAP && "$gap" == "-" ]]; then bad "$id gap row needs a gap_note"; fi
    check_bug "$id" "$dir" "$expect" "$oracle" "$env_spec"
    check_fixed "$id" "$dir"
    ts_status=off; cs_status=off; py_status=off
    if [[ "$OTHERS" == 1 ]]; then check_others "$id" "$dir" "$cs_obs"; fi
    by_expect[$expect]=$(( ${by_expect[$expect]:-0} + 1 ))
    note "$id $level/$expect ts=$ts_status cs=$cs_status py=$py_status"
done <"$MANIFEST"

[[ "$rows" -eq 16 ]] || bad "expected 16 manifest rows, found $rows"
if [[ "${PGY_CORPUS_DEMO-1}" == 1 ]]; then
    PGY_BIN="$PGY" bash tests/counterexample_corpus/demo_agenthub/demo.sh >"$WORK/demo.log" 2>&1 ||
        bad "AgentHub demo failed ($WORK/demo.log)"
    note "$(tail -1 "$WORK/demo.log")"
fi
summary=""
for key in REJECT_COMPILE TRAP_RUNTIME RUNTIME_SAFE ACCEPTED_GAP; do
    summary="$summary $key=${by_expect[$key]:-0}"
done
note "rows=$rows$summary evidence=$WORK"
if [[ "$fail_count" -ne 0 ]]; then
    note "$fail_count failure(s)"
    exit 1
fi
note "PASS (Rust is not part of this run; the corpus records it as not yet compared)"

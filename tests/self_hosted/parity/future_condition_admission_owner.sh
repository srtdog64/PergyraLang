#!/usr/bin/env bash
# Constant/nonconstant flow must keep the same affine completion obligations.
# Invalid programs are admission-only inputs and are never compiled or run.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here future-condition "$PGY"
pgy_require_runnable_binary_here future-condition "$DRIVER"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/future-condition.XXXXXX)"
sha256sum "$PGY" "$DRIVER" >"$WORK/inputs.sha256"
checks=0
while IFS='|' read -r name accepted native_code body; do
    source="$WORK/$name.pgy"
    {
        printf '%s\n' 'async func Work(x: Int) -> Int { return x * 3; }'
        printf '%s\n' 'func Flag() -> Bool { return true; }'
        printf '%s\n' 'async func Main() -> Void { let t: Future<Int> = spawn Work(14);'
        printf '%s\n' "$body" '}'
    } >"$source"
    for origin in native self; do
        command=("$DRIVER" --emit-mir-json-verified "$source")
        diagnostic='Code: task_lifecycle_invalid'
        if [[ "$origin" == native ]]; then
            command=("$PGY" --native-pipeline --mir-json --error-format=json "$source")
            diagnostic="$native_code"
        fi
        status=0
        timeout 30 "${command[@]}" >"$WORK/$name.$origin.out" 2>"$WORK/$name.$origin.err" || status=$?
        if [[ "$accepted" == yes ]]; then
            [[ "$status" == 0 ]] && grep -Fq '"pgy.mir.v1"' "$WORK/$name.$origin.out" || {
                echo "[future-condition] FAIL $name/$origin valid admission" >&2; exit 1;
            }
        else
            [[ "$status" == 1 ]] &&
                grep -Fq "$diagnostic" "$WORK/$name.$origin.out" "$WORK/$name.$origin.err" &&
                ! grep -Fq '"pgy.mir.v1"' "$WORK/$name.$origin.out" || {
                echo "[future-condition] FAIL $name/$origin owned refusal" >&2; exit 1;
            }
        fi
        checks=$((checks + 1))
    done
done <<'CASES'
true-retire|yes||if true { let v: Int = await t; Log(ToString(v)); }
false-keep|yes||if false { let v: Int = await t; Log(ToString(v)); } let v: Int = await t; Log(ToString(v));
false-loop-keep|yes||while false { await t; } let v: Int = await t; Log(ToString(v));
false-leak|no|PGY_SEM_TASK_LIFECYCLE|if false { let v: Int = await t; Log(ToString(v)); }
dynamic-leak|no|PGY_SEM_TASK_LIFECYCLE|if Flag() { let v: Int = await t; Log(ToString(v)); }
double-retire|no|PGY_SEM_MOVE_FROM_RELEASED|if true { let v: Int = await t; Log(ToString(v)); } let v: Int = await t; Log(ToString(v));
CASES
echo "[future-condition] $checks native/self admission controls PASS; evidence: $WORK"

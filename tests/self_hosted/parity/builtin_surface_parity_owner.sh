#!/usr/bin/env bash
# Native C (--native-pipeline --backend=c) and the default C route accept the
# same builtin surface with the same meaning. Every row of
# src/common/pgy_builtin_type_table.c, plus EXTRA_ROWS, has one call program
# in builtin_surface_shapes.txt; the gate compiles it on both legs and
# requires one of:
#   - both accept, and the binaries print the same output and exit status;
#   - both refuse, and the row is in BOTH_REFUSED_ROWS with its reason;
#   - native accepts, the default route refuses with the explicit
#     builtin_native_pipeline_only diagnostic naming the builtin, and the row
#     is in NATIVE_ONLY_ROWS with its reason.
# Anything else fails, including a default-route acceptance of a builtin
# native refuses. A table row with no shape fails, so a new builtin cannot
# skip the matrix. NATIVE_ONLY_ROWS only shrinks: its size is pinned, and it
# must equal the self-host list in native_pipeline_only_builtin_owner.pgy.
#
# The gate also pins the diagnostics of an unresolved builtin inside `&&` in
# argument position and in an unannotated let: a user error, never an
# internal artifact failure (ast_artifact_invalid).
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="builtin-surface-parity"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/builtin_surface_parity"
WORK_DIR="$ROOT_DIR/$WORK_REL"
TABLE="$ROOT_DIR/src/common/pgy_builtin_type_table.c"
SHAPES="$ROOT_DIR/tests/self_hosted/parity/builtin_surface_shapes.txt"
SELF_OWNER="$ROOT_DIR/src/self_hosted/semantic/native_pipeline_only_builtin_owner.pgy"
JOBS="${PGY_BUILTIN_SURFACE_JOBS:-8}"
SURVEY="${PGY_BUILTIN_SURFACE_SURVEY:-0}"
NATIVE_ONLY_CODE="builtin_native_pipeline_only"

# Builtins outside the table that the matrix also holds to one surface.
EXTRA_ROWS="UnwrapErr UnwrapOr"

# Rows neither front end accepts: name|reason.
BOTH_REFUSED_ROWS='
CompilerRetireArrayStorage|compiler-internal: both routes admit it only inside the self-host storage lifetime owner
Length|a type-table row for the .Length member; neither route declares a callable Length()
'

# Rows native accepts and the default route refuses with the native-only
# diagnostic: name|reason[|first]. The diagnostic names the row, or `first`
# when the smallest valid program must call that native-only row earlier
# (IntoClassical needs a claimed, measured qubit). Shrink-only: remove a row
# when the default route accepts it, drop it from
# native_pipeline_only_builtin_owner.pgy, and lower NATIVE_ONLY_PINNED.
NATIVE_ONLY_ROWS='
ChannelCapacity|the default route has no channel runtime (a <- receive is refused)
ChannelClose|the default route has no channel runtime (a <- receive is refused)
ChannelClosed|the default route has no channel runtime (a <- receive is refused)
ChannelFull|the default route has no channel runtime (a <- receive is refused)
ChannelLength|the default route has no channel runtime (a <- receive is refused)
ChannelReady|the default route has no channel runtime (a <- receive is refused)
ChannelSpace|the default route has no channel runtime (a <- receive is refused)
ClaimQubit|experimental qubit surface; the default route has no qubit runtime
CooldownNew|untyped state tool (native types it Unknown); no default-route runtime
FsmNew|untyped state tool (native types it Unknown); no default-route runtime
H|experimental qubit surface; the default route has no qubit runtime
HasLayer|zone query; only the rung2 intent driver resolves domain queries, the installed default route does not
HasProjection|zone query; only the rung2 intent driver resolves domain queries, the installed default route does not
HasState|zone state query; no self-host domain-query protocol row
HasZone|world zone query; no self-host domain-query protocol row
HasZoneLayer|world query; only the rung2 intent driver resolves domain queries, the installed default route does not
HasZoneProjection|world query; only the rung2 intent driver resolves domain queries, the installed default route does not
HasZoneState|world state query; no self-host domain-query protocol row
Input|prompted console read; the default route has no Input runtime
IntoClassical|experimental qubit surface; the default route has no qubit runtime|ClaimQubit
IsCollapsed|experimental qubit surface; the default route has no qubit runtime
IsNone|the self-host types Option builtins by name before resolution (TYPED_PROTOCOL, docs/205 R7); IsNone is shadowable, so it waits for the declared-callable fact
Measure|experimental qubit surface; the default route has no qubit runtime
QubitState|experimental qubit surface; the default route has no qubit runtime
SendTimeout|the default route has no channel runtime (a <- receive is refused)
TimerNew|untyped state tool (native types it Unknown); no default-route runtime
TrySend|the default route has no channel runtime (a <- receive is refused)
'
NATIVE_ONLY_PINNED=27

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
[[ -f "$SHAPES" ]] || fail "missing shapes table $SHAPES"
[[ -f "$SELF_OWNER" ]] || fail "missing self-host native-only owner $SELF_OWNER"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR/rows"
printf 'stdin-line\n' > "$WORK_DIR/stdin.txt"

# --- rows, shapes and lists -----------------------------------------------
sed -nE 's/^[[:space:]]*\{ "([A-Za-z0-9]+)", "[^"]*",$/\1/p; s/^[[:space:]]*\{ "([A-Za-z0-9]+)", "[^"]*", PGY_BUILTIN_FLAG_[A-Z_]+ \},$/\1/p' \
    "$TABLE" > "$WORK_DIR/table.names"
[[ "$(wc -l < "$WORK_DIR/table.names")" -gt 100 ]] ||
    fail "builtin table extraction found too few rows"
{ cat "$WORK_DIR/table.names"; printf '%s\n' $EXTRA_ROWS; } |
    sort > "$WORK_DIR/rows.names"
[[ -z "$(uniq -d "$WORK_DIR/rows.names")" ]] ||
    fail "a row is listed twice: $(uniq -d "$WORK_DIR/rows.names" | tr '\n' ' ')"

awk -v dir="$WORK_DIR/rows" '
    /^== / { name = $2; file = dir "/" name ".pgy"; print name; next }
    name == "" { next }
    { print > file }
' "$SHAPES" | sort > "$WORK_DIR/shape.names"
[[ -z "$(uniq -d "$WORK_DIR/shape.names")" ]] ||
    fail "a shape is written twice: $(uniq -d "$WORK_DIR/shape.names" | tr '\n' ' ')"
missing="$(comm -23 "$WORK_DIR/rows.names" "$WORK_DIR/shape.names" | tr '\n' ' ')"
[[ -z "$missing" ]] || fail "builtin rows with no call shape: $missing"
stale="$(comm -13 "$WORK_DIR/rows.names" "$WORK_DIR/shape.names" | tr '\n' ' ')"
[[ -z "$stale" ]] || fail "shapes for names that are not rows: $stale"

list_names() { printf '%s\n' "$1" | sed -n 's/^\([A-Za-z0-9]*\)|..*$/\1/p' | sort; }
list_names "$BOTH_REFUSED_ROWS" > "$WORK_DIR/both_refused.names"
list_names "$NATIVE_ONLY_ROWS" > "$WORK_DIR/native_only.names"
for list in both_refused native_only; do
    [[ -z "$(uniq -d "$WORK_DIR/$list.names")" ]] || fail "$list lists a row twice"
    extra="$(comm -23 "$WORK_DIR/$list.names" "$WORK_DIR/rows.names" | tr '\n' ' ')"
    [[ -z "$extra" ]] || fail "$list names rows that do not exist: $extra"
done
both="$(comm -12 "$WORK_DIR/both_refused.names" "$WORK_DIR/native_only.names" | tr '\n' ' ')"
[[ -z "$both" ]] || fail "rows in both lists: $both"
native_only_count="$(wc -l < "$WORK_DIR/native_only.names" | tr -d ' ')"
[[ "$native_only_count" -le "$NATIVE_ONLY_PINNED" ]] ||
    fail "NATIVE_ONLY_ROWS grew to $native_only_count rows (pinned $NATIVE_ONLY_PINNED); the list only shrinks"
[[ "$native_only_count" -ge "$NATIVE_ONLY_PINNED" ]] ||
    fail "NATIVE_ONLY_ROWS shrank to $native_only_count rows; lower NATIVE_ONLY_PINNED to match"
sed -n '/^func SemanticNativePipelineOnlyBuiltinNames/,/^}/p' "$SELF_OWNER" |
    sed -nE 's/^[[:space:]]*"([A-Za-z0-9]+)",?$/\1/p' | sort > "$WORK_DIR/self_native_only.names"
cmp -s "$WORK_DIR/native_only.names" "$WORK_DIR/self_native_only.names" || {
    diff "$WORK_DIR/native_only.names" "$WORK_DIR/self_native_only.names" >&2 || true
    fail "NATIVE_ONLY_ROWS differs from native_pipeline_only_builtin_owner.pgy"
}
in_list() { grep -Fxq "$2" "$WORK_DIR/$1.names"; }
# The builtin a row's native-only refusal names: the row, or its `first` field.
native_only_callee() {
    local first
    first="$(printf '%s\n' "$NATIVE_ONLY_ROWS" | sed -n "s/^$1|[^|]*|\([A-Za-z0-9]*\)\$/\1/p")"
    printf '%s\n' "${first:-$1}"
}
while read -r name; do
    callee="$(native_only_callee "$name")"
    [[ "$callee" == "$name" ]] || in_list native_only "$callee" ||
        fail "$name is refused at $callee, which is not a NATIVE_ONLY_ROWS row"
done < "$WORK_DIR/native_only.names"

# --- run one row on one leg -----------------------------------------------
# Writes rows/<name>.<leg>.{rc,log,out,status}: rc is the compile status,
# out/status the binary's stdout and exit status when it compiled.
run_leg() {
    local name="$1" leg="$2" flags=()
    case "$leg" in
        native-c) flags=(--native-pipeline --backend=c) ;;
        default-c) flags=(--backend=c) ;;
        *) echo "unknown leg $leg" >&2; return 2 ;;
    esac
    local base="$WORK_DIR/rows/$name.$leg"
    local run_dir="$base.run"
    mkdir -p "$run_dir"
    local rc=0
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$WORK_REL/rows/$name.pgy" "${flags[@]}" \
        -o "$WORK_REL/rows/$name.$leg.exe") >"$base.log" 2>&1 || rc=$?
    echo "$rc" > "$base.rc"
    [[ "$rc" == 0 ]] || return 0
    local status=0
    (cd "$run_dir" && "$WORK_DIR/rows/$name.$leg.exe" \
        <"$WORK_DIR/stdin.txt" >"$base.raw" 2>/dev/null) || status=$?
    tr -d '\r' <"$base.raw" >"$base.out"
    echo "$status" > "$base.status"
}

running=0
while read -r name; do
    for leg in native-c default-c; do
        run_leg "$name" "$leg" &
        running=$(( running + 1 ))
        if [[ "$running" -ge "$JOBS" ]]; then
            wait
            running=0
        fi
    done
done < "$WORK_DIR/rows.names"
wait

# --- classify -------------------------------------------------------------
names_callee() { grep -Eq "func: $2([^A-Za-z0-9]|\$)" "$1"; }

accepted=0
refused=0
native_only=0
: > "$WORK_DIR/errors"
: > "$WORK_DIR/matrix"
while read -r name; do
    n="$WORK_DIR/rows/$name.native-c"
    d="$WORK_DIR/rows/$name.default-c"
    n_rc="$(cat "$n.rc")"
    d_rc="$(cat "$d.rc")"
    if [[ "$n_rc" == 0 && "$d_rc" == 0 ]]; then
        if cmp -s "$n.out" "$d.out" && cmp -s "$n.status" "$d.status"; then
            verdict="accepted"
        else
            verdict="output-differs"
        fi
    elif [[ "$n_rc" != 0 && "$d_rc" != 0 ]]; then
        verdict="both-refused"
    elif [[ "$n_rc" == 0 ]]; then
        if grep -Fq "Code: $NATIVE_ONLY_CODE" "$d.log" &&
            names_callee "$d.log" "$(native_only_callee "$name")"; then
            verdict="native-only"
        else
            verdict="default-refused"
        fi
    else
        verdict="native-refused"
    fi
    printf '%s %s\n' "$name" "$verdict" >> "$WORK_DIR/matrix"
    if [[ "$SURVEY" == 1 ]]; then
        printf '%-28s %-16s | default: %s\n' "$name" "$verdict" \
            "$(grep -m1 -E 'Code:|error' "$d.log" 2>/dev/null | tr -s ' ' | cut -c1-100 || true)"
        continue
    fi
    case "$verdict" in
        accepted)
            accepted=$(( accepted + 1 ))
            ! in_list both_refused "$name" ||
                echo "$name: both routes accept it; remove it from BOTH_REFUSED_ROWS" >> "$WORK_DIR/errors"
            ! in_list native_only "$name" ||
                echo "$name: the default route accepts it; remove it from NATIVE_ONLY_ROWS and the self-host owner, and lower the pin" >> "$WORK_DIR/errors"
            ;;
        both-refused)
            refused=$(( refused + 1 ))
            in_list both_refused "$name" ||
                echo "$name: both routes refuse its call shape (see $WORK_REL/rows/$name.*.log)" >> "$WORK_DIR/errors"
            ;;
        native-only)
            native_only=$(( native_only + 1 ))
            in_list native_only "$name" ||
                echo "$name: refused as native-only but NATIVE_ONLY_ROWS does not list it" >> "$WORK_DIR/errors"
            ;;
        output-differs)
            echo "$name: native C and the default route print different results (diff $WORK_REL/rows/$name.native-c.out $WORK_REL/rows/$name.default-c.out)" >> "$WORK_DIR/errors"
            ;;
        default-refused)
            echo "$name: native accepts it and the default route refuses it without $NATIVE_ONLY_CODE (see $WORK_REL/rows/$name.default-c.log)" >> "$WORK_DIR/errors"
            ;;
        native-refused)
            echo "$name: the default route accepts a builtin native refuses (see $WORK_REL/rows/$name.native-c.log)" >> "$WORK_DIR/errors"
            ;;
    esac
done < "$WORK_DIR/rows.names"
[[ "$SURVEY" != 1 ]] || exit 0

if [[ -s "$WORK_DIR/errors" ]]; then
    sed "s/^/[$LABEL] /" "$WORK_DIR/errors" >&2
    exit 1
fi

# --- diagnostics of an unresolved builtin -----------------------------------
# expect_default_refusal TAG CODE CALLEE PROGRAM
expect_default_refusal() {
    local tag="$1" code="$2" callee="$3" program="$4"
    printf '%s\n' "$program" > "$WORK_DIR/diag-$tag.pgy"
    local log="$WORK_DIR/diag-$tag.log"
    if (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$WORK_REL/diag-$tag.pgy" --backend=c \
        -o "$WORK_REL/diag-$tag.exe") >"$log" 2>&1; then
        fail "the default route accepted the $tag program"
    fi
    if ! grep -Fq "Code: $code" "$log" || ! names_callee "$log" "$callee" ||
        grep -Fq "ast_artifact_invalid" "$log"; then
        cat "$log" >&2
        fail "the $tag program did not report $code for $callee"
    fi
}
and_argument() {
    printf '%s\n' \
        'func Check(name: String, cond: Bool) -> Int { if cond { return 0; } return 1; }' \
        'func Main() -> Void {' \
        '    let s: String = "hello err";' \
        "    Log(ToString(Check(\"x\", $1(s, \"hello\") && $1(s, \"err\"))));" \
        '}'
}
unannotated_let() {
    printf '%s\n' 'func Main() -> Void {' "    let c = $1(\"ab\", \"a\");" \
        '    Log("x");' '}'
}
native_only_probe="$(head -1 "$WORK_DIR/native_only.names")"
# StartsWith is a builtin on neither route, so it stays undefined.
expect_default_refusal and-undefined undefined_function StartsWith "$(and_argument StartsWith)"
expect_default_refusal let-undefined undefined_function StartsWith "$(unannotated_let StartsWith)"
if [[ -n "$native_only_probe" ]]; then
    expect_default_refusal and-native-only "$NATIVE_ONLY_CODE" "$native_only_probe" \
        "$(and_argument "$native_only_probe")"
    expect_default_refusal let-native-only "$NATIVE_ONLY_CODE" "$native_only_probe" \
        "$(unannotated_let "$native_only_probe")"
fi

echo "[$LABEL] $accepted rows accepted on both routes with equal output, $refused refused on both, $native_only native-only (pinned $NATIVE_ONLY_PINNED): PASS"

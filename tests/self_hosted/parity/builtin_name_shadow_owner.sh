#!/usr/bin/env bash
# A top-level function owns a builtin's spelling (docs/205 R7). Native used to
# resolve a builtin before any user function, so `func Max(..)` compiled to
# the builtin while the default route called the function or refused it.
#
# Every builtin spelling is now in exactly one of two classes, the same on
# native C/LLVM and the default C/LLVM route:
#   - shadowable (tests/self_hosted/fixtures/builtin_name_shadow_names.txt):
#     one program declares a function for every name, and each call, in
#     expression and statement position, runs the program function;
#   - reserved (src/semantic/builtin_name_reservation.def plus the capability
#     registry): the declaration is refused with no binary.
# The gate also holds the two reservation tables to each other, the runtime
# ABI family to the runtime's declarations, and every builtin table row to a
# class.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="builtin-name-shadow"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/builtin_name_shadow"
WORK_DIR="$ROOT_DIR/$WORK_REL"
NATIVE_DEF="$ROOT_DIR/src/semantic/builtin_name_reservation.def"
CAPABILITY_DEF="$ROOT_DIR/src/semantic/builtin_capability_registry.def"
SELF_OWNER="$ROOT_DIR/src/self_hosted/semantic/builtin_shadow_owner.pgy"
SHADOW_NAMES="$ROOT_DIR/tests/self_hosted/fixtures/builtin_name_shadow_names.txt"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

# --- tables ---------------------------------------------------------------
sed -nE 's/^PGY_BUILTIN_NAME_RESERVED\(([A-Z_]+), "([A-Za-z0-9]+)"\)$/\1^\2/p' \
    "$NATIVE_DEF" > "$WORK_DIR/native.rows"
grep -oE '"[A-Z_]+\^[A-Za-z0-9]+"' "$SELF_OWNER" | tr -d '"' > "$WORK_DIR/self.rows"
[[ -s "$WORK_DIR/native.rows" ]] || fail "native reservation table is empty"
cmp -s "$WORK_DIR/native.rows" "$WORK_DIR/self.rows" ||
    fail "self-host reservation rows differ from builtin_name_reservation.def"
sed -nE 's/^PGY_BUILTIN_CAPABILITY\([A-Z_]+, [0-9]+, "([A-Za-z0-9]+)".*/\1/p' \
    "$CAPABILITY_DEF" > "$WORK_DIR/capability.names"
[[ -s "$WORK_DIR/capability.names" ]] || fail "capability registry is empty"
cut -d'^' -f2 "$WORK_DIR/native.rows" | cat - "$WORK_DIR/capability.names" |
    sort > "$WORK_DIR/reserved.names"
[[ -z "$(uniq -d "$WORK_DIR/reserved.names")" ]] ||
    fail "a name is reserved twice: $(uniq -d "$WORK_DIR/reserved.names" | tr '\n' ' ')"
sort "$SHADOW_NAMES" > "$WORK_DIR/shadow.names"
[[ -z "$(comm -12 "$WORK_DIR/reserved.names" "$WORK_DIR/shadow.names")" ]] ||
    fail "a name is both reserved and shadowable"

runtime_declares() {
    grep -Eqh "^[A-Za-z_][A-Za-z0-9_ *]*[ *]$1[[:space:]]*\(" \
        "$ROOT_DIR"/src/runtime/*.h "$ROOT_DIR"/src/runtime/*.c
}
while IFS='^' read -r family name; do
    [[ "$family" == RUNTIME_ABI ]] || continue
    runtime_declares "$name" || fail "RUNTIME_ABI row '$name' is not a runtime declaration"
done < "$WORK_DIR/native.rows"
while read -r name; do
    ! runtime_declares "$name" ||
        fail "shadowable '$name' is declared by the runtime; reserve it as RUNTIME_ABI"
done < "$WORK_DIR/shadow.names"

# Every row of the builtin tables is classified.
{
    sed -nE 's/^[[:space:]]*\{"([A-Za-z0-9]+)", BUILTIN_[A-Z_]+\},$/\1/p' \
        "$ROOT_DIR/src/semantic/type_checker_builtins_resolve.c"
    sed -nE 's/^[[:space:]]*\{ "([A-Za-z0-9]+)", "[^"]*", PGY_BUILTIN_FLAG_[A-Z_]+ \},$/\1/p' \
        "$ROOT_DIR/src/common/pgy_builtin_type_table.c"
    sed -n '/^func SemanticBuiltinSignatureRows/,/^}/p' \
        "$ROOT_DIR/src/self_hosted/semantic/builtin_signature_owner.pgy" |
        sed -nE 's/^[[:space:]]*"([A-Za-z0-9]+)\^.*/\1/p'
} | sort -u > "$WORK_DIR/table.names"
[[ "$(wc -l < "$WORK_DIR/table.names")" -gt 100 ]] || fail "builtin table extraction found too few rows"
sort -u "$WORK_DIR/reserved.names" "$WORK_DIR/shadow.names" > "$WORK_DIR/classified.names"
unclassified="$(comm -23 "$WORK_DIR/table.names" "$WORK_DIR/classified.names" | tr '\n' ' ')"
[[ -z "$unclassified" ]] || fail "unclassified builtin names: $unclassified"

# --- shadowable names run the program function ----------------------------
PROGRAM="$WORK_DIR/shadow.pgy"
EXPECTED="$WORK_DIR/shadow.expected"
: > "$PROGRAM"
: > "$EXPECTED"
index=0
while read -r name; do
    base=$(( (index + 1) * 1000 ))
    printf 'func %s(x: Int) -> Int {\n    Log(x + %d);\n    return x + %d;\n}\n\n' \
        "$name" "$(( base + 100 ))" "$(( base + 41 ))" >> "$PROGRAM"
    printf '%d\n%d\n%d\n%d\n%d\n' "$(( base + 101 ))" "$(( base + 42 ))" \
        "$(( base + 103 ))" "$(( base + 102 ))" "$(( base + 43 ))" >> "$EXPECTED"
    index=$(( index + 1 ))
done < "$SHADOW_NAMES"
{
    printf 'func Main() -> Void {\n'
    index=0
    while read -r name; do
        printf '    let r%d: Int = %s(1);\n    Log(r%d);\n    %s(3);\n    Log(%s(2));\n' \
            "$index" "$name" "$index" "$name" "$name"
        index=$(( index + 1 ))
    done < "$SHADOW_NAMES"
    printf '}\n'
} >> "$PROGRAM"

for leg in native-c native-llvm default-c default-llvm; do
    case "$leg" in
        native-c) flags=(--native-pipeline --backend=c) ;;
        native-llvm) flags=(--native-pipeline --backend=llvm) ;;
        default-c) flags=(--backend=c) ;;
        default-llvm) flags=(--backend=llvm) ;;
    esac
    out_rel="$WORK_REL/shadow-$leg.exe"
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$WORK_REL/shadow.pgy" "${flags[@]}" -o "$out_rel") \
        >"$WORK_DIR/shadow-$leg.log" 2>&1 ||
        fail "$leg refused the shadowing program (see $WORK_REL/shadow-$leg.log)"
    "$ROOT_DIR/$out_rel" </dev/null 2>&1 | tr -d '\r' >"$WORK_DIR/shadow-$leg.out" ||
        fail "$leg shadowing program exited non-zero"
    cmp -s "$EXPECTED" "$WORK_DIR/shadow-$leg.out" ||
        fail "$leg did not run every program function (diff $WORK_REL/shadow.expected $WORK_REL/shadow-$leg.out)"
done

# --- reserved names are refused, one per family ---------------------------
for entry in CAPABILITY:Now RUNTIME_ABI:Sqrt RESOURCE:Read STATEMENT_FORM:Log CONSTRUCTOR:Ok TYPED_PROTOCOL:MapKeys; do
    family="${entry%%:*}"
    name="${entry#*:}"
    grep -Fxq "$name" "$WORK_DIR/reserved.names" || fail "sample '$name' is not reserved"
    printf 'func %s(x: Int) -> Int {\n    return x;\n}\n\nfunc Main() -> Void {\n    let r: Int = %s(1);\n}\n' \
        "$name" "$name" > "$WORK_DIR/reserved-$name.pgy"
    for leg in native-c native-llvm default-c default-llvm; do
        case "$leg" in
            native-c) flags=(--native-pipeline --backend=c); want="is reserved by the $family builtin family" ;;
            native-llvm) flags=(--native-pipeline --backend=llvm); want="is reserved by the $family builtin family" ;;
            default-c) flags=(--backend=c); want="builtin_name_reserved" ;;
            default-llvm) flags=(--backend=llvm); want="builtin_name_reserved" ;;
        esac
        out_rel="$WORK_REL/reserved-$name-$leg.exe"
        if (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
            "$PGY" "$WORK_REL/reserved-$name.pgy" "${flags[@]}" -o "$out_rel") \
            >"$WORK_DIR/reserved-$name-$leg.log" 2>&1; then
            fail "$leg accepted a function named $name"
        fi
        [[ ! -e "$ROOT_DIR/$out_rel" ]] || fail "$leg left a binary for the reserved name $name"
        grep -Fq -- "$want" "$WORK_DIR/reserved-$name-$leg.log" ||
            fail "$leg refused $name for another reason (want: $want)"
    done
done

echo "[$LABEL] $(wc -l < "$WORK_DIR/shadow.names") shadowable names run the program function and 6 reserved families refuse, on native and default C/LLVM: PASS"

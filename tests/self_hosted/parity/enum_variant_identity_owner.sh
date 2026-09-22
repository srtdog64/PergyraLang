#!/usr/bin/env bash
# An enum variant name has one declaration (docs/205 L3a). Two enums declaring
# the same variant, a variant reusing a function name, and `Enum.Variant(..)`
# naming another enum's variant used to resolve to whichever declaration came
# first. Each is now refused with no binary on native C/LLVM and on the
# default C/LLVM route.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-enum-variant-identity"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/enum_variant_identity"
WORK_DIR="$ROOT_DIR/$WORK_REL"
FIXTURES="tests/self_hosted/fixtures"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

# name:fixture:native diagnostic fragment:default-route diagnostic fragment
# (the default route refuses a variant that reuses a function name while it
# builds call-target facts, before this rule runs, so that row only needs the
# refusal itself there).
CASES=(
    "duplicate:$FIXTURES/enum_variant_duplicate_negative.pgy:Enum variant 'Red' of 'Paint' reuses a name:enum_variant_redeclaration"
    "function:$FIXTURES/enum_variant_function_name_negative.pgy:Enum variant 'Bad' of 'Verdict' reuses a name:Status: error"
    "qualifier:$FIXTURES/enum_variant_foreign_qualifier_negative.pgy:'Bad' is not a variant of enum 'Shape':undefined_function"
)
for entry in "${CASES[@]}"; do
    IFS=: read -r name fixture native_text default_text <<<"$entry"
    for leg in native-c native-llvm default-c default-llvm; do
        case "$leg" in
            native-c) flags=(--native-pipeline --backend=c); want="$native_text" ;;
            native-llvm) flags=(--native-pipeline --backend=llvm); want="$native_text" ;;
            default-c) flags=(--backend=c); want="$default_text" ;;
            default-llvm) flags=(--backend=llvm); want="$default_text" ;;
        esac
        out_rel="$WORK_REL/$name-$leg.exe"
        if (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
            "$PGY" "$fixture" "${flags[@]}" -o "$out_rel") \
            >"$WORK_DIR/$name-$leg.log" 2>&1; then
            fail "$leg accepted the $name variant program"
        fi
        [[ ! -e "$ROOT_DIR/$out_rel" ]] ||
            fail "$leg left a binary for the refused $name variant program"
        grep -Fq -- "$want" "$WORK_DIR/$name-$leg.log" ||
            fail "$leg refused the $name variant program for another reason (want: $want)"
    done
done

echo "[$LABEL] duplicate, function-colliding and foreign-qualified variants are refused on native and default C/LLVM: PASS"

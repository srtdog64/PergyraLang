#!/usr/bin/env bash
# Native and self-host admission both refuse one binding passed to two inout
# parameters of the same call, on every backend, before any MIR or binary is
# published. Distinct inout arguments and an inout+value pair on one binding
# stay admitted and run with the expected values. A field or element place
# passed inout is refused on every route with no binary: native semantic
# names it, the default C route stops at codegen, and the default LLVM route
# stops at direct MIR admission.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-inout-argument-alias"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/inout_argument_alias"
WORK_DIR="$ROOT_DIR/$WORK_REL"
FIXTURES="tests/self_hosted/fixtures"

# name:fixture:binding named by the native diagnostic
NEGATIVE_CASES=(
    "adjacent:$FIXTURES/direct_mir_inout_argument_alias_negative.pgy:a"
    "forwarded:$FIXTURES/direct_mir_inout_argument_alias_forwarded_negative.pgy:total"
)
# name:fixture:expected stdout with lines joined by '|'
POSITIVE_CASES=(
    "distinct:$FIXTURES/direct_mir_inout_argument_distinct_parameter.pgy:1|10"
    "inout-value:$FIXTURES/direct_mir_inout_value_same_binding_parameter.pgy:10"
)

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

for entry in "${NEGATIVE_CASES[@]}"; do
    IFS=: read -r name fixture binding <<<"$entry"

    native_c_rel="$WORK_REL/native-$name.c"
    if (cd "$ROOT_DIR" && "$PGY" "$fixture" \
        --native-pipeline --emit-c -o "$native_c_rel") \
        >"$WORK_DIR/native-$name.out" 2>"$WORK_DIR/native-$name.err"; then
        fail "native semantic accepted $name"
    fi
    [[ ! -e "$ROOT_DIR/$native_c_rel" ]] ||
        fail "native published C for rejected $name"
    grep -Fq "'$binding' is passed to multiple inout parameters" \
        "$WORK_DIR/native-$name.err" ||
        fail "native $name lost its inout alias diagnostic"

    self_mir_rel="$WORK_REL/self-$name.mir.json"
    if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
        "$fixture" -o "$self_mir_rel") \
        >"$WORK_DIR/self-$name.out" 2>"$WORK_DIR/self-$name.err"; then
        fail "self-host semantic accepted $name"
    fi
    [[ ! -e "$ROOT_DIR/$self_mir_rel" ]] ||
        fail "self-host published MIR for rejected $name"
    grep -Fq 'inout_argument_alias' \
        "$WORK_DIR/self-$name.out" "$WORK_DIR/self-$name.err" ||
        fail "self-host $name lost its stable diagnostic identity"
    cat "$WORK_DIR/self-$name.out" "$WORK_DIR/self-$name.err" | tr -d '\r' |
        grep -Fx -- "- binding: $binding" >/dev/null ||
        fail "self-host $name does not name the aliased binding"

    # The default route delegates to the installed driver; both backends
    # must stop at the same semantic refusal, not at a backend limit.
    for backend in c llvm; do
        binary_rel="$WORK_REL/default-$name-$backend.exe"
        if (cd "$ROOT_DIR" && "$PGY" "$fixture" --backend="$backend" \
            -o "$binary_rel") \
            >"$WORK_DIR/default-$name-$backend.out" \
            2>"$WORK_DIR/default-$name-$backend.err"; then
            fail "default $backend route accepted $name"
        fi
        [[ ! -e "$ROOT_DIR/$binary_rel" ]] ||
            fail "default $backend route published a binary for $name"
        grep -Fq 'inout_argument_alias' \
            "$WORK_DIR/default-$name-$backend.out" \
            "$WORK_DIR/default-$name-$backend.err" ||
            fail "default $backend route refused $name for another reason"
    done
done

# name:fixture
PLACE_CASES=(
    "field:$FIXTURES/inout_field_place_negative.pgy"
    "element:$FIXTURES/inout_element_place_negative.pgy"
)
for entry in "${PLACE_CASES[@]}"; do
    IFS=: read -r name fixture <<<"$entry"
    for leg in native-c native-llvm default-c default-llvm; do
        case "$leg" in
            native-c) flags=(--native-pipeline --backend=c); want="An inout argument must be a variable" ;;
            native-llvm) flags=(--native-pipeline --backend=llvm); want="An inout argument must be a variable" ;;
            default-c) flags=(--backend=c); want="inout argument must be a variable" ;;
            default-llvm) flags=(--backend=llvm); want="" ;;
        esac
        binary_rel="$WORK_REL/place-$name-$leg.exe"
        if (cd "$ROOT_DIR" && "$PGY" "$fixture" "${flags[@]}" -o "$binary_rel") \
            >"$WORK_DIR/place-$name-$leg.log" 2>&1; then
            fail "$leg accepted an inout $name place"
        fi
        [[ ! -e "$ROOT_DIR/$binary_rel" ]] ||
            fail "$leg published a binary for an inout $name place"
        [[ -z "$want" ]] || grep -Fq "$want" "$WORK_DIR/place-$name-$leg.log" ||
            fail "$leg refused the inout $name place for another reason"
    done
done

for entry in "${POSITIVE_CASES[@]}"; do
    IFS=: read -r name fixture expected <<<"$entry"
    for backend in c llvm; do
        binary_rel="$WORK_REL/default-$name-$backend.exe"
        (cd "$ROOT_DIR" && "$PGY" "$fixture" --backend="$backend" \
            -o "$binary_rel") \
            >"$WORK_DIR/default-$name-$backend.out" \
            2>"$WORK_DIR/default-$name-$backend.err" || {
                cat "$WORK_DIR/default-$name-$backend.out" \
                    "$WORK_DIR/default-$name-$backend.err" >&2
                fail "$name control was rejected on the default $backend route"
            }
        actual="$("$ROOT_DIR/$binary_rel" | tr -d '\r' | paste -sd '|' -)"
        [[ "$actual" == "$expected" ]] ||
            fail "$name on $backend printed '$actual', expected '$expected'"
    done
done

echo "[$LABEL] native/self-host refusal on C and LLVM, field and element place refusal, distinct and inout+value controls: PASS"

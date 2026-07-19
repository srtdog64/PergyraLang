#!/usr/bin/env bash
# Mechanism reachability gate: no mechanism without a consumer, or an explicit
# declaration that it has none.
#
# The contract SoT is src/self_hosted/compiler/reachability_owner.pgy. This gate
# proves, in order:
#   1. the runnable manifest reproduces the pinned golden contract (C leg, via
#      the self-hosted backend-output comparator), so the table and its
#      in-language invariants hold;
#   2. the LLVM-compiled manifest is artifact-equal to the C-compiled one;
#   3. the CENSUS: for every declared row, count the files under its consumer
#      scope -- excluding its own home -- that reference its entry symbol as a
#      whole word, and check that count against the declared status:
#         live           >= 1  (losing the last consumer is silent death)
#         declared_only  == 0  (gaining one means the declaration is stale)
#   4. self-test in both directions: the census must report zero for a symbol
#      that does not exist, and non-zero for one that certainly does. A census
#      that always answers the same thing would pass step 3 vacuously.
#
# Why the asymmetry in step 3. `live` is a floor so ordinary work can add
# consumers freely. `declared_only` is exact, because those rows ARE the known
# gaps -- the AIR execution-lane fact that codegen drops, and the M:N fiber
# scheduler that is compiled into the binary with no caller. Making them exact
# means such a gap can neither widen unnoticed nor be quietly closed without
# updating the record of why it existed.
#
# Usage: PGY_BIN=bin/pgy.exe bash tests/selfhost_reachability_contract_smoke.sh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="selfhost-reachability-contract"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[$LABEL] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[$LABEL] missing compiler binary: $PGY" >&2
    exit 1
fi

MANIFEST_SOURCE="$ROOT_DIR/src/self_hosted/compiler/reachability_manifest.pgy"
OWNER_SOURCE="$ROOT_DIR/src/self_hosted/compiler/reachability_owner.pgy"
EXPECTED_FILE="$ROOT_DIR/src/self_hosted/compiler/expected_reachability_manifest.txt"
for path in "$MANIFEST_SOURCE" "$OWNER_SOURCE" "$EXPECTED_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[$LABEL] missing input: $path" >&2
        exit 1
    fi
done

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/reachability_contract}"
mkdir -p "$BUILD_DIR"

TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$MANIFEST_SOURCE")"
C_BIN="$BUILD_DIR/reachability_c.exe"
C_COMPILE_LOG="$BUILD_DIR/reachability_c.compile.log"
C_OUT="$BUILD_DIR/reachability_c.out"
C_ERR="$BUILD_DIR/reachability_c.err"

if ! (cd "$ROOT_DIR" && "$PGY" "$TOOL_ARG" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$C_BIN")" >"$C_COMPILE_LOG" 2>&1); then
    echo "[$LABEL] C compile failed" >&2
    cat "$C_COMPILE_LOG" >&2
    exit 1
fi

set +e
(cd "$ROOT_DIR" && "$C_BIN" 2>"$C_ERR" | pgy_selfhost_normalize_text_artifact >"$C_OUT")
C_RC=$?
set -e
if [[ "$C_RC" -ne 0 ]]; then
    echo "[$LABEL] C-compiled manifest failed" >&2
    cat "$C_OUT" "$C_ERR" >&2
    exit 1
fi

# 1) golden diff through the self-hosted backend-output comparator
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "$LABEL" "$BUILD_DIR" "$EXPECTED_FILE" "$C_OUT" "reachability_contract"

# 2) backend invariance of the contract tool itself
assert_llvm_leg "$LABEL" "$TOOL_ARG" "$BUILD_DIR"

# 3) the census
# Files under $scope, excluding those under $home, that mention $symbol as a
# whole word. -w matters: pgy_parallel_chunk_count must not be satisfied by
# pgy_parallel_chunk_count_export, which is a different symbol.
consumer_count() { # $1 = symbol, $2 = home prefix, $3 = scope prefix
    local symbol="$1" home="$2" scope="$3" hits n
    if [[ ! -d "$ROOT_DIR/$scope" ]]; then
        echo "[$LABEL] declared scope does not exist: $scope" >&2
        exit 1
    fi
    # Zero hits is a legitimate answer here (it is what declared_only asserts),
    # but grep exits 1 on no-match and this script runs under `set -e` with
    # pipefail -- so the empty case must be taken before any pipeline runs, and
    # the filtering grep needs its own `|| true`.
    # --include MUST precede `--`: after `--` grep treats them as file operands,
    # the C-source filter silently stops applying, and the census then counts
    # this very contract's owner and golden as "consumers" of every symbol they
    # name. The sentinel probe below is what pins that behaviour.
    hits="$(grep -rlw --include=*.c --include=*.h -- "$symbol" "$ROOT_DIR/$scope" \
        2>/dev/null || true)"
    if [[ -z "$hits" ]]; then
        echo 0
        return 0
    fi
    n="$(printf '%s\n' "$hits" | sed "s|^$ROOT_DIR/||" | grep -cv "^$home" || true)"
    echo "${n:-0}" | tr -d '[:space:]'
}

rows=0
live_rows=0
declared_rows=0
# `|| [[ -n ... ]]`: the normalized artifact has no trailing newline.
while IFS='|' read -r kind name symbol home scope status || [[ -n "${kind:-}" ]]; do
    [[ "$kind" == "reach" ]] || continue
    if [[ -z "$name" || -z "$symbol" || -z "$home" || -z "$scope" || -z "$status" ]]; then
        echo "[$LABEL] malformed reach row: $kind|$name|$symbol|$home|$scope|$status" >&2
        exit 1
    fi
    count="$(consumer_count "$symbol" "$home" "$scope")"
    case "$status" in
        live)
            if [[ "$count" -lt 1 ]]; then
                echo "[$LABEL] '$name' is declared live but has NO consumer." >&2
                echo "[$LABEL]   symbol $symbol, scope $scope, home $home" >&2
                echo "[$LABEL]   the mechanism died silently: either restore a" >&2
                echo "[$LABEL]   consumer or move the row to declared_only with a note." >&2
                exit 1
            fi
            live_rows=$((live_rows + 1))
            ;;
        declared_only)
            if [[ "$count" -ne 0 ]]; then
                echo "[$LABEL] '$name' is declared_only but now has $count consumer(s)." >&2
                echo "[$LABEL]   symbol $symbol, scope $scope, home $home" >&2
                echo "[$LABEL]   a known gap closed -- that is good news, but the" >&2
                echo "[$LABEL]   declaration and its note must say so. Move the row" >&2
                echo "[$LABEL]   to live and record what changed." >&2
                exit 1
            fi
            declared_rows=$((declared_rows + 1))
            ;;
        *)
            echo "[$LABEL] unknown status '$status' for row $name" >&2
            exit 1
            ;;
    esac
    rows=$((rows + 1))
done <"$C_OUT"

if [[ "$rows" -lt 8 ]]; then
    echo "[$LABEL] expected at least 8 reach rows, saw $rows" >&2
    exit 1
fi
if [[ "$live_rows" -lt 1 || "$declared_rows" -lt 1 ]]; then
    echo "[$LABEL] the table lost a polarity (live=$live_rows declared_only=$declared_rows);" >&2
    echo "[$LABEL] with only one kind of row this gate stops testing half of itself." >&2
    exit 1
fi

# 4) self-test, both directions. A census that always answers zero would make
# every live row fail; one that always answers non-zero would make step 3
# vacuous for the declared_only rows. Prove it can do both.
SENTINEL="definitely_missing_reachability_contract_symbol"
if ! grep -qF -- "$SENTINEL" "$OWNER_SOURCE"; then
    echo "[$LABEL] self-test sentinel disappeared from the owner" >&2
    exit 1
fi
# The sentinel exists in exactly one place: this contract's own .pgy owner.
# Censusing it with a home that does NOT cover that owner therefore probes two
# things at once -- that an absent-from-C symbol reports zero, AND that the
# C-source include filter is really applied. If --include ever stops working,
# the owner file matches and this returns 1.
zero_probe="$(consumer_count "$SENTINEL" "src/runtime/" "src")"
if [[ "$zero_probe" -ne 0 ]]; then
    echo "[$LABEL] self-test broken: a symbol that appears only in the .pgy owner" >&2
    echo "[$LABEL] reported $zero_probe C-source consumers -- the --include filter" >&2
    echo "[$LABEL] is not being applied, so every census result is meaningless." >&2
    exit 1
fi
nonzero_probe="$(consumer_count "pgy_spawn" "src/runtime/" "src")"
if [[ "$nonzero_probe" -lt 1 ]]; then
    echo "[$LABEL] self-test broken: a symbol known to have consumers reported zero;" >&2
    echo "[$LABEL] the census is not actually searching (every live row would pass" >&2
    echo "[$LABEL] or fail for the wrong reason)." >&2
    exit 1
fi

echo "[$LABEL] ok (golden contract + llvm-leg parity + census over $rows mechanisms:" \
     "$live_rows live, $declared_rows declared_only + two-way self-test)"

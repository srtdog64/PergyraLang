#!/usr/bin/env bash
set -euo pipefail

# CLOSED fallback identities owned by this gate:
# parser_local_capability_table, parser_local_effect_table,
# ast_print_mask_name_table, semantic_effect_word_table,
# runtime_capability_word_table, mir_json_mask_name_table,
# selfhost_contract_vocabulary_array,
# mir_lower_chained_vocabulary_predicate, literal_known_mask_union,
# duplicate_clause_word_success, local_nonlocal_mix_success,
# projection_drift.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REGISTRY="$ROOT_DIR/src/semantic/callable_contract_vocabulary.def"
PROJECTION="$ROOT_DIR/src/self_hosted/lib/callable_contract_vocabulary_projection_owner.pgy"
RUNTIME_PROJECTION="$ROOT_DIR/src/runtime/pgy_callable_contract_capability_projection.h"
BUILD_DIR="$ROOT_DIR/.tmp/callable_contract_vocabulary"
PYTHON_BIN="${PYTHON_BIN:-python3}"
CC_BIN="${CC:-cc}"
PGY_BIN="${PGY_BIN:-$ROOT_DIR/bin/pgy.exe}"

mkdir -p "$BUILD_DIR"

"$PYTHON_BIN" "$ROOT_DIR/scripts/render_callable_contract_vocabulary.py" \
    "$REGISTRY" "$PROJECTION" "$RUNTIME_PROJECTION" --check

"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])
sys.path.insert(0, str(root / "scripts"))
import render_callable_contract_vocabulary as registry

rows = registry.load_rows(root / "src/semantic/callable_contract_vocabulary.def")
caps = [row for row in rows if row.axis == "capability"]
effects = [row for row in rows if row.axis == "effect"]
assert len(caps) == 9 and len(effects) == 9
assert [row.canonical_rank for row in caps] == list(range(9))
assert [row.canonical_rank for row in effects] == list(range(9))
assert len({row.stable_id for row in rows}) == 18
assert len({row.spelling for row in rows}) == 18
assert [row.spelling for row in effects if row.zero_policy == registry.ZERO_EXCLUSIVE] == ["local"]
assert not {"all", "none"} & {row.spelling for row in caps}
PY

"$CC_BIN" -std=c11 -Wall -Wextra -Werror -I"$ROOT_DIR/src" \
    "$ROOT_DIR/tests/callable_contract_vocabulary_probe.c" \
    "$ROOT_DIR/src/semantic/callable_contract_vocabulary.c" \
    -o "$BUILD_DIR/callable_contract_vocabulary_probe.exe"
"$BUILD_DIR/callable_contract_vocabulary_probe.exe"

if [[ -x "$PGY_BIN" ]]; then
    (cd "$ROOT_DIR" && "$PGY_BIN" \
        tests/cases/callable_contract_vocabulary/valid_all/main.pgy \
        --emit-c -o .tmp/callable_contract_vocabulary/valid_all.c) \
        >"$BUILD_DIR/valid_all.out" 2>"$BUILD_DIR/valid_all.err"
    while IFS='|' read -r case_name diagnostic; do
        if (cd "$ROOT_DIR" && "$PGY_BIN" \
            "tests/cases/callable_contract_vocabulary/$case_name/main.pgy" \
            --emit-c -o ".tmp/callable_contract_vocabulary/$case_name.c") \
            >"$BUILD_DIR/$case_name.out" 2>"$BUILD_DIR/$case_name.err"; then
            echo "[callable-contract-vocabulary] invalid source accepted: $case_name" >&2
            exit 1
        fi
        grep -Fq "$diagnostic" \
            "$BUILD_DIR/$case_name.out" "$BUILD_DIR/$case_name.err" || {
            echo "[callable-contract-vocabulary] diagnostic drifted: $case_name" >&2
            cat "$BUILD_DIR/$case_name.out" "$BUILD_DIR/$case_name.err" >&2
            exit 1
        }
    done <<'CASES'
duplicate_cap|Duplicate capability 'io_read'
duplicate_effect|Duplicate effect 'secure'
local_mixed_first|Effect 'local' cannot be combined with other effects
local_mixed_last|Effect 'local' cannot be combined with other effects
CASES
fi

grep -Fq '#include "pgy_callable_contract_capability_projection.h"' \
    "$ROOT_DIR/src/runtime/pgy_runtime_capability.h"
grep -Fq 'CallableContractVocabularyFindIndex(' \
    "$ROOT_DIR/src/self_hosted/parser/function_decl_owner.pgy"
grep -Fq 'CallableContractVocabularyFindIndex(' \
    "$ROOT_DIR/src/self_hosted/semantic/ast_action_contract_fact_owner.pgy"
grep -Fq 'CallableContractVocabularyFindIndex(' \
    "$ROOT_DIR/src/self_hosted/mir/declaration_verify_owner.pgy"
grep -Fq 'pgy_callable_contract_vocabulary_find(' \
    "$ROOT_DIR/src/parser/parser_decl_clause.c"
grep -Fq 'pgy_callable_contract_vocabulary_known_mask(' \
    "$ROOT_DIR/src/compiler/mir_decl_header_validate.c"

for forbidden in \
    'kEffectWordSpecs' \
    'static const CapBitName k_cap_names' \
    'static const ASTPrintMaskName effects[]' \
    'static const MIRJsonMaskName caps[]' \
    'MirDeclarationMethodContractKnownCap' \
    'MirDeclarationMethodContractKnownEffect'; do
    if rg -Fq "$forbidden" \
        "$ROOT_DIR/src/parser" \
        "$ROOT_DIR/src/semantic" \
        "$ROOT_DIR/src/compiler" \
        "$ROOT_DIR/src/self_hosted/parser/function_decl_owner.pgy" \
        "$ROOT_DIR/src/self_hosted/semantic/ast_action_contract_fact_owner.pgy" \
        "$ROOT_DIR/src/self_hosted/mir" \
        "$ROOT_DIR/src/self_hosted/mir_lower"; then
        echo "[callable-contract-vocabulary] forbidden local vocabulary returned: $forbidden" >&2
        exit 1
    fi
done

echo "[callable-contract-vocabulary] 18 semantic words and projections: ok"

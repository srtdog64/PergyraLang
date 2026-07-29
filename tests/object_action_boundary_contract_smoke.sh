#!/usr/bin/env bash
# Pins the source-backed struct/class/object/tobject/vessel/subject/action
# boundary. This is a documentation/owner ratchet, not executable self-host
# substitution evidence.
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 2

DOC="docs/200_object_to_action_boundary_patterns.md"
DOGFOOD_DOC="docs/199_language_word_and_dogfood_grammar.md"
fail=0

require_text() {
    if ! grep -qF "$2" "$1"; then
        echo "[FAIL] $1 no longer contains: $2"
        fail=1
    fi
}

reject_text() {
    if grep -qF "$2" "$1"; then
        echo "[FAIL] $1 reintroduced forbidden text: $2"
        fail=1
    fi
}

if [ ! -f "$DOC" ]; then
    echo "[FAIL] missing canonical boundary contract: $DOC"
    exit 1
fi

# Canonical authoring decisions and the deliberately open tobject debt.
require_text "$DOC" '현재 구현과 canonical authoring 행렬'
require_text "$DOC" '일반 vessel 파라미터도 포인터로 전달한다'
require_text "$DOC" 'receiver mode와'
require_text "$DOC" 'parameter carriage를 합치면'
require_text "$DOC" '`subject` | identity-bearing active host'
require_text "$DOC" 'canonical surface는 receiver 없음'
require_text "$DOC" '현재 semantic은 passive `func`를 허용하지만 새 코드는 method-free'
require_text "$DOC" '`func == pure`, `action == impure`로 나누지'
require_text "$DOC" '`causes DamageEffect` 같은 domain effect'
require_text "$DOC" '`MakeSubject().Action()` 같은 temporary subject receiver'
require_text "$DOC" 'C-owned compiler path를 대체한 `SUBSTITUTING`은 아니다'
require_text "$DOC" 'Artifact action의 commit 조건'
require_text "$DOC" 'Begin(final path)'
require_text "$DOC" '`DriverRung2Executed(receipt)`'
require_text "$DOC" '`tobject SelfMirArtifactReceipt`'
require_text "$DOC" '`crash_durable`은 항상 `false`'
require_text "$DOC" '`pgy.mir.v1` declaration JSON은'
require_text "$DOC" '상속 계층이 아니라 경계 프로토콜이다'
require_text "$DOC" '재사용 가능한 다섯 경계 패턴'
require_text "$DOC" '`subject source -> object slot -> refresh -> query`'
require_text "$DOC" '`subject source -> tobject slot -> publish -> transfer/receipt`'
require_text "$DOC" '`subject -> owned vessel -> hosted func`'
require_text "$DOC" '`typed input fact -> subject.action -> Result/stage/tobject receipt`'
require_text "$DOC" '`ArrayPush(result.rows, ...)`처럼 live mutable builder로 사용하지 않는다'
require_text "$DOC" 'target-neutral plan이 C/LLVM의 마지막 소비자 앞에서 한 번 만들어진다'
require_text "$DOC" 'import되거나 parser fixture에 나온다는 사실은 이 패턴이 실행된 증거가 아니다'
require_text "$DOC" '`tobject` | source lifecycle에서 분리된 immutable transfer value'
require_text "$DOC" '`effect` | participant에 적용·유지되는 시간적 상태 layer'
require_text "$DOC" '`relation` | 두 participant identity 사이의 materialized edge'
require_text "$DOC" '모든 stage를 ceremonial'
require_text "$DOC" 'IMPORTED -> MATERIALIZED -> INVOKED -> OUTCOME_CONSUMED -> SUBSTITUTING'
require_text "$DOC" '`MIRDeclField`에는 현재 mutability fact도 없다'
require_text "$DOC" 'Field reference의 최소 join key는'
require_text "$DOC" 'Raw ID 숫자의 native/self equality는 계약이 아니다'
require_text "$DOC" '`VesselSlot:`으로 출력할 수 있지만'
reject_text "$DOC" '`vessel`은 일반 함수 인자로는 값 전달되지만'
reject_text "$DOGFOOD_DOC" 'self-host parser가 action contract의 caps/effects를 보존하지 않고'
require_text "$DOGFOOD_DOC" 'requires/within/causes/authorized/caps/effects declaration contract를 운반한다'

# The parser owns six distinct nominal identities; aliases may not collapse them.
for kind in CLASS SUBJECT VESSEL STRUCT OBJECT TOBJECT; do
    require_text "src/parser/ast_types.h" "NOMINAL_DECL_${kind}"
done
require_text "src/parser/parser_decl.c" \
    'decl_kind == NOMINAL_DECL_SUBJECT'
require_text "src/parser/parser_decl.c" \
    'parser_decl_match_contextual_keyword(parser, "action")'

# Semantic and backend facts behind the matrix.
require_text "src/semantic/type_checker_class_decl.c" \
    "struct '%s' cannot have methods"
require_text "src/semantic/type_checker_module_contract.c" \
    "action '%s' is only supported inside subject declarations"
require_text "src/codegen/host_decl_compat.c" \
    'ast_class_nominal_kind(decl) == NOMINAL_DECL_SUBJECT'
require_text "src/codegen/host_decl_compat.c" \
    '|| ast_class_nominal_kind(decl) == NOMINAL_DECL_VESSEL'
require_text "src/semantic/type_checker_assignment.c" \
    "object '%s' fields are read-only after construction"
require_text "src/semantic/type_checker_assignment.c" \
    "tobject '%s' fields are immutable"
require_text "src/semantic/type_checker_assignment.c" \
    'transfer snapshots must be republished from their source'
require_text "src/semantic/type_checker_domain_slots.c" \
    'only refresh/publish/bind owns projection source identity'
require_text "src/semantic/type_checker_domain_slots.c" \
    'PGY_CAUSE_DOMAIN_PROJECTION_INITIALIZER'
require_text "src/lexer/language_keyword_registry.def" \
    '"binding", PGY_KEYWORD_CLASS_CONTEXTUAL'
require_text "src/parser/parser_domain.c" \
    'slot->data.domain_slot.is_binding = is_subject || is_binding;'
require_text "src/self_hosted/parser/decl_zone_owner.pgy" \
    'slot_label = "BindingSlot";'
require_text "src/self_hosted/semantic/nominal_constructor_argument_policy_owner.pgy" \
    'kind == NominalFieldKindBindingSlot()'
require_text "src/compiler/mir_domain_topology.c" \
    'MIR_TOPOLOGY_FIELD_SUBJECT | MIR_TOPOLOGY_FIELD_BINDING'
topology_owner="src/compiler/mir_domain_topology.c"
generic_layer_matcher="$(awk \
    '/mir_domain_topology_field_kind_matches\(/,/^}/' \
    "$topology_owner")"
apply_layer_matcher="$(awk \
    '/mir_domain_topology_apply_effect_layer_identity_matches\(/,/^}/' \
    "$topology_owner")"
apply_layer_consumer="$(awk \
    '/} else if \(row->kind == MIR_DOMAIN_TOPOLOGY_APPLY_EFFECT/,/} else if \(row->kind == MIR_DOMAIN_TOPOLOGY_LINK_RELATION/' \
    "$topology_owner")"
if ! grep -qF 'return !mir_decl_field_is_pool_layer(field)' \
    <<<"$generic_layer_matcher"; then
    echo "[FAIL] generic MIR topology layer matching must reject pool layers"
    fail=1
fi
if ! grep -qF '== MIR_DECL_FIELD_ZONE_LAYER_SLOT' \
    <<<"$apply_layer_matcher" \
    || ! grep -qF '!mir_decl_field_is_relation_layer(field)' \
        <<<"$apply_layer_matcher" \
    || grep -qF 'mir_decl_field_is_pool_layer(field)' \
        <<<"$apply_layer_matcher"; then
    echo "[FAIL] APPLY_EFFECT layer identity must admit exact effect slots, including pools"
    fail=1
fi
if ! grep -qF 'row->kind == MIR_DOMAIN_TOPOLOGY_APPLY_EFFECT' \
    <<<"$apply_layer_consumer" \
    || ! grep -qF 'mir_domain_topology_apply_effect_layer_identity_matches(' \
        <<<"$apply_layer_consumer" \
    || ! grep -qF 'MIR_TOPOLOGY_FIELD_EFFECT' \
        <<<"$apply_layer_consumer"; then
    echo "[FAIL] APPLY_EFFECT must be the sole pool-admitting topology consumer"
    fail=1
fi
if [ "$(grep -cF 'mir_domain_topology_apply_effect_layer_identity_matches(' \
        "$topology_owner")" -ne 2 ]; then
    echo "[FAIL] APPLY_EFFECT pool matcher escaped its definition plus sole consumer"
    fail=1
fi
require_text "src/compiler/mir_decl_field_kind_vocabulary.def" \
    'BINDING_SLOT, 5, "binding_slot", "BindingSlot"'
require_text "src/self_hosted/mir/domain_runtime_assignment_fact_owner.pgy" \
    'rows.field_kinds[field] == NominalFieldKindBindingSlot()'
require_text "src/self_hosted/dir/domain_graph_fact_owner.pgy" \
    'NominalFieldKindBindingSlot()'
require_text "src/self_hosted/dir/domain_topology_row_owner.pgy" \
    'field_name, NominalFieldKindBindingSlot()'
require_text "$DOC" \
    '`binding slot`은 object-valued endpoint를 zone에 들이는 명시적 admission 역할이다.'
require_text "$DOC" '실제 실행에서는 zero-filled storage로'
require_text "src/codegen/llvm_type.c" \
    'return kind == NOMINAL_DECL_OBJECT || kind == NOMINAL_DECL_TOBJECT;'
require_text "src/codegen/llvm_type.c" \
    'return kind == NOMINAL_DECL_TOBJECT;'
require_text "src/semantic/type_checker_host_helpers.c" \
    'type_is_class_object_type(const Type *type, SemanticContext *ctx)'
require_text "$DOC" '`type_is_class_object_type()`은 이름과 달리 현재 subject만'

# Raw file handles remain a compatibility surface, never an artifact action
# receipt. Native semantic/runtime mode gates are closed, while AIR FileOpen
# stays explicitly partial until MIR carries the mode-derived fact.
require_text "src/runtime/pgy_runtime_file_mode_capability.h" \
    'pgy_file_mode_capability_mask(const char *mode)'
require_text "src/semantic/type_checker_builtins_nominal.c" \
    'pgy_file_mode_capability_mask('
require_text "src/semantic/type_checker_builtins_nominal.c" \
    'semantic_record_capability(ctx, PGY_CAP_IO_WRITE);'
require_text "src/runtime/pgy_runtime_io_qubit_inline.h" \
    'PGY_CAP_IO_WRITE, "file-open-write"'
require_text "src/runtime/pgy_runtime_io_qubit_inline.h" \
    'PGY_CAP_IO_READ, "file-read"'
require_text "src/runtime/pgy_runtime_io_qubit_inline.h" \
    'PGY_CAP_IO_WRITE, "file-write"'
require_text "src/runtime/pgy_runtime_lib_io_string_exports.h" \
    'PGY_CAP_IO_WRITE, "file-open-write"'
reject_text "src/semantic/capability_analyze.c" \
    '{"FileOpen",'

# Keep the present-vs-canonical tobject distinction honest until semantic closure.
require_text "src/tests/semantic/test_semantic_misc_b1_part_a_2.cases.h" \
    'object and tobject declarations may carry passive helper funcs'

# Stale prose that previously made the language model self-contradictory.
reject_text "docs/grammar/02_grammar.md" 'subject (`class` alias)'
reject_text "docs/grammar/00_cheatsheet.md" '시그니처 장식'
reject_text "docs/grammar/00_cheatsheet.md" 'with effects io_read, clock'
reject_text "docs/26_vessel_action_model.md" \
    '읽기 전용 계산 (value-self, mutation 없음)'
reject_text "docs/10_role_interface_design.md" \
    '별도 코어 타입은 아니다'

if [ "$fail" -eq 0 ]; then
    echo "ALL PASS (object-to-action boundary contract)"
    exit 0
fi

echo "FAILED"
exit 1

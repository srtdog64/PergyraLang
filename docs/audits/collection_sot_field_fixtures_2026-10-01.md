# Collection field-lifetime fixture preparation

Status: PREPARATION ONLY; every `.pgy` below is UNEXECUTED. No parser, compiler,
MIR producer, backend, runtime, parity gate, installed driver, or CI result is
claimed. Unsafe negative programs must never execute, including after accidental
compiler admission. This report and the fixtures do not close a SoT row.

Date: 2026-10-01, Asia/Seoul. Directive base:
`5cf41dcf5a974d19fad42ae3b3fd7ace24035be7`. Observed moving-tree HEAD:
`d7abecfb76902368dee180ece99fc31a704f281b`. Immediately before creation there
were 66 short-status entries; existing dirty work was preserved. Recheck HEAD,
status, source hashes, and main's recorded seed before integrating these drafts.

Scope B adds only this report and new files under
`docs/audits/repros/collection_sot_2026-10-01/`. No source, runtime, existing
test, Makefile, registry, handoff, installed binary or shared temporary evidence
was edited; no build, compile, fixture execution, stage, commit or push ran.

## Purpose and current owners

Prepare the reached aggregate-formal field falsifier together with legitimate
callable-table cleanup controls. Stable binding/field identity and actual
producer-to-release lifetime take priority over refusal count. The current
Pergyra collection verdict, member identity and transition owners must provide
the proof; the native admission remains a consumer/oracle with an explicit
UNKNOWN exemption. The integration owner is `pergyraLang 메인`; the proposed
single integration gate remains
`tests/self_hosted/parity/collection_ownership_semantic_owner.sh`, after carrier
validation. No new implementation track is opened here.

Inspected source facts:

- `src/semantic/collection_ownership_fact.c:781-803` rejects UNKNOWN member
  extraction without element proof, but lines 788-790 still admit it when the
  source binding is a current aggregate parameter. The borrowed formal-field
  negatives below are falsifiers of that exemption, not already observed rejects.
- `src/self_hosted/semantic/ast_collection_ownership_member_move_owner.pgy:33-41`
  resolves a scoped local root. It has no demonstrated formal-root identity join.
  A spelling such as `value` and an `inout` mode cannot fill that gap.
- `src/self_hosted/semantic/ast_collection_ownership_verdict_owner.pgy:294-321`
  checks `SemanticAstCollectionOwnershipBuiltinCall` before dispatching ownership
  operations and names `borrow_boundary_escape` for a deep drop lacking binding
  provenance. Neither function spelling nor type alone proves builtin identity.
  These drafts declare no user callable with either builtin name.
- `src/self_hosted/semantic/ast_expression_function_table_fact_owner.pgy:6-18`
  defines `ok/names/returns/params`; Ready checks `ok` and equal lengths only.
  Its real Release at lines 31-43 extracts three fields, deep drops them, writes
  the retired descriptors back, and sets `ok=false`. Ready and `ok` are shape
  checks, not String-element ownership evidence.
- `src/self_hosted/semantic/ast_expression_environment_owner.pgy:107-152,256-305`
  and `src/self_hosted/semantic/builtin_signature_owner.pgy:237-239` construct
  owned rows using the actual `ArrayPushOwnedString` builtin, including builtin,
  constructor, user-function, enum and intent contributions. The draft populated
  release control uses this builtin directly; it does not invent a clone helper.

## Minimal control packet

All paths in this table are relative to
`docs/audits/repros/collection_sot_2026-10-01/`.

| Draft | Intended obligation, not observed result |
|---|---|
| `bundle_field_byvalue_borrowed_negative.pgy` | Empty local + actual shallow `ArrayPush` borrows a literal, then constructor -> by-value `Bundle.value` formal -> named local -> actual deep-drop builtin. This adds transition-produced borrowed provenance to the existing aggregate-literal by-value fixture. Require explicit refusal before backend execution. |
| `bundle_field_inout_borrowed_negative.pgy` | The same caller provenance reaches an `inout Bundle.value`; extracted descriptor is written back as in the real release owner. `inout` must not grant String lifetime. Require explicit refusal before backend execution. |
| `callable_table_borrowed_negative.pgy` | Equal-length borrowed literal fields plus `ok=true` satisfy Ready but flow to the imported real Release. Require caller/formal/field provenance refusal; accepting by table type/name/Ready is the falsifying case. Never run this program. |
| `callable_table_empty_release_positive.pgy` | Manual empty table -> real Release -> all lengths zero and `ok=false`. Release-component positive only. |
| `callable_table_owned_release_positive.pgy` | Three empty arrays populated by actual `ArrayPushOwnedString` -> manual table -> real Release -> zero lengths and `ok=false`. Release-component positive only. |
| `hashmap_normal_exit_positive.pgy` | One `MapNew/MapSet/MapGet` local reaches Main's closing brace, with no explicit drop or early return. Require ordinary cleanup exactly once and output `1`. |
| `callable_table_producer_input.pgy` | Small actual source input with a constructor and one typed user callable; input for the next row, not production compiler coverage by itself. |
| `callable_table_from_artifact_release_probe.pgy` | Real `ParseRootProgramArtifact` -> existing signature/constructor/enum owners -> real FromArtifact -> checked user row -> real Release. Expected success output is `producer-table-retired`; UNEXECUTED producer-component candidate only. |

Existing controls were inspected before creation:
`collection_parameter_member_move_deep_drop.pgy` already covers by-value
`Bundle.values` from an aggregate literal;
`collection_parameter_direct_field_deep_drop.pgy` tests an unnamed direct field;
`inout_string_array_deep_drop.pgy` tests a bare array formal; and
`empty_owned_string_push_drop_valid.pgy` tests local ownership without aggregate
carriage. They are retained, not copied as extra variants. The new field pair
uses a shallow-push transition and adds aggregate `inout` plus writeback.

The current semantic gate executes the early-return HashMap fixture with
`MapHas=true` and output `1` (`collection_ownership_semantic_owner.sh:544-580`).
It also counts emitted normal-exit release sites, but that execution does not
reach the closing-brace path. The new MapGet control supplies that unexecuted
normal-exit path without replacing existing map storage/key tests.

## Real producer candidate and remaining production proof

The probe API is grounded in
`tests/self_hosted/semantic/fixture/compiler_internal_builtin_artifact_provenance.pgy`,
which uses actual source parsing, and in production
`ast_artifact_verdict_owner.pgy:157-176`, which obtains signatures with builtin
names, constructors and enums and calls the real FromArtifact owner. The new
probe follows those APIs; it does not synthesize AST text or a fake production
artifact. Main must first compile it with an identified current-source pair and
confirm all imported definitions and parser behavior. A drafted source call is
not proof that its producer ran.

Even a future green producer-component probe will leave this required chain
unproved: real FromArtifact String allocation -> field declaration identity ->
aggregate construction/return -> caller binding -> formal binding identity ->
Release extraction -> single deep retirement -> writeback/released state -> last
consumer rejection. No demonstrated admitted ownership receipt carrying that
whole chain was observed during this preparation. Its owner is the existing
Pergyra collection verdict/identity/transition seam, not Ready, an `inout`
exception, or this report. The borrowed-table counterexample specifically
prevents granting every table of this nominal type the same release authority.

Actual DRV-2 retirement remains mandatory:
`src/self_hosted/compiler/driver_rung2_owner.pgy:178-186` releases the MIR
projection's analysis table, and its source-C route at lines 373-382 retires the
table only after `CodegenAdmittedCViewFromFactsOrDie`. The source fact owner's
comment mentioning body analysis does not authorize moving that later boundary.
The new component probe reaches neither consumer. Main must pair real
source-to-MIR/source-C acceptance and required C/LLVM carrier/cleanup evidence
with the borrowed negatives before deleting the native exemption. No blanket
formal-root refusal is acceptable if it breaks this legitimate production path.

Expected diagnostic ownership is a requirement, not an asserted emitted string:
`borrow_boundary_escape` / native `PGY_CODE_SEM_BORROW_ESCAPE` come from the
existing verdict/native admission. The aggregate current-parameter exemption
means that exact rejection stage and detailed reason remain UNVERIFIED.

## Inspected input hash boundary

These SHA-256 values matched the directive snapshot at the observed `d7abecfb`
HEAD. Working-tree content, not HEAD alone, identifies the inspected owners.

```text
2C1FE4058BA60DD4300913D805E5A53A9D48B0D198EA5942EB463DE472DDAA39  src/semantic/collection_ownership_fact.c
3504AB775DB78CA434399CE5BCB9000501D92A15805EA68D1FD4B3DD51B9E02E  src/self_hosted/semantic/ast_collection_ownership_member_move_owner.pgy
CDD2DD93093A40BC64573093B2EB7E9C20269B838B6709529FD9C3B1F0C614F4  src/self_hosted/semantic/ast_expression_function_table_fact_owner.pgy
F3EDDEF784F5B08FFE2B5BB06742E1A2A456CC1430133376987C9EEB7E38E04F  src/self_hosted/semantic/ast_expression_environment_owner.pgy
85D206EE7ABBAEAB5B95B1F227B4EB063697847336007D0BE26CE265BD345B9D  src/self_hosted/semantic/ast_collection_ownership_verdict_owner.pgy
66F4F4C065449E224705BD2F762082490120A0F032E597E5F0C91F4A8C634DF2  tests/self_hosted/parity/collection_ownership_semantic_owner.sh
3B07A6A1867CACBF220FF06071D1E43A761335255CED65AB8EBCC3859110CBA3  docs/semantics/sot_owner_spine_registry.md
F2E1252C539965B5276C3AFC82C6E57E125FF6122B2C61F08B87E7776E68EEBA  src/self_hosted/parser/program_parse_owner.pgy
34569399CFB3FCB2512C5DFA40278A67F5F7E9AEA8ECC4A95AED9FC085A1CAF8  src/self_hosted/semantic/ast_artifact_verdict_owner.pgy
5E759D63E2246F63DCF203CB9606C13D449E0EF9B4456BE82F8A3FE030FC11F0  src/self_hosted/semantic/builtin_signature_owner.pgy
9429C47E150CD57298682AE8EAF83AE884DECBCC4B500327B5B2B8EE0520C5B5  src/self_hosted/compiler/driver_rung2_owner.pgy
```

Only static path/reference and whitespace checks are permitted for this packet.
There is no last-green language gate attributable to these drafts.

Final static observation: the eight new files exist, all carry `UNEXECUTED`,
every import resolves to an existing file, and none has trailing whitespace.
`git diff --check` passed for the tracked diff; a separate content check covered
these untracked fixture files. The report's producer API names were found in
current source. HEAD remained `d7abecfb76902368dee180ece99fc31a704f281b`, the
seven directive hashes remained equal, and the dirty short-status count was 68
at that observation. Parallel outputs may subsequently change that count. No
language-syntax or behavior validation is implied by those static checks.

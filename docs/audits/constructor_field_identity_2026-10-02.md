# Constructor-field identity: bounded continuation evidence

Base: `53168fe98acd6f9490cf70404a383a83bfe43f8e`.
This is an audit, not a semantic authority, lifetime permission or SoT closure.
Directive: `../agent_work_directives/constructor_field_identity_2026-10-02.md`.

## Observed implementation

The existing constructor argument policy alone maps supplied argument ordinal
to storage field index, excluding domain-managed object/tobject/layer fields.
The new one-argument admission fact binds contextual type and canonical field
SyntaxId from that row. Call admission retains only scalar constructor identity.
No extra per-call field-ID vector, element ownership or release permission is
added. The attempted vector was removed before delivery: it was constructed
twice for root calls, unused by production consumers and lacked success cleanup.

The call-tree precheck now consumes the same nominal admission result instead
of imposing ordinary exact arity first. Source signatures exclude a declared
function even before late identity sealing; formal/local callable and runtime
identity remain excluded. The ordinary call path keeps exact arity. The call
tree retains each node's missing-fact refusal while visiting its children.
A missing nested callee owns the more specific diagnostic; valid children do
not erase their parent's refusal, even when a parent Core never revisits it.

No caps were raised. Comment-excluded counts, with blank lines included:
argument policy 100/100; call owner 150/160; argument owner 61/100;
expression verdict 599/600. No installed binary or companion manifest changed.

## Executed evidence

- `.tmp/self_hosted/nominal-constructor-field-identity.qyeuJz`: native C and
  LLVM each execute 31 current-owner identity/type/shadow checks. Domain field
  gaps, same-type argument reversal, prefix/empty/nonempty Array/Set, invalid
  type/arity, missing/zero field identity, malformed column/span and late/early
  declared callable identities are covered. These are developer harnesses.
- The same gate analyzes eight actual source inputs with current parser/body
  owners: nominal prefix and ordinary exact invocation accepted; same-spelling
  ordinary prefix refused with call_arity_mismatch; missing nested call refused
  with undefined_function. Direct/nested heterogeneous-array arguments both
  refuse with ast_artifact_invalid; nested prefix remains accepted and nested
  Missing() remains undefined_function. Inputs are never emitted or executed.
- Frozen pre-phase developer probe refused the valid nominal prefix with
  call_arity_mismatch; the current source probe accepts it. A temporary current
  precheck changed nested Missing() to ast_artifact_invalid. Its initial
  correction discarded every nominal artifact error, and the frozen .0yCux0
  analyzer wrongly admitted [Bag([1, "x"])] while refusing its direct form.
  Parallel source review found this case; root analyzed it and replaced that
  skip with a retained node-local error. The final gate preserves both the
  refusal and the pre-phase undefined_function diagnostic.
- `.tmp/self_hosted/nominal-field-write-fact.NV2IFu`: unchanged field-write
  provenance/missing-row/diagnostic owner executes 13 checks on both C and LLVM.
- `.tmp/self_hosted/constructor-lifetime-check.WGpj9Q`: current developer probe
  still refuses seven borrowed field cases and admits five ordinary controls.
  All three actual table-release positives remain refused. This matrix is
  partial/HELD, not a green aggregate lifetime integration result.
- Source/input/probe hashes and unchanged native/installed-driver hashes were
  rechecked by the gates. Native driver remains A5DFE2CB…3936; installed
  self-driver remains 707DCD40…EC7; manifest remains 0A83B0DB…C19.

- Isolated staged snapshot `.tmp/self_hosted/constructor-staged.aIxqx2`:
  twenty changed files were index-blob equal before documentation refresh.
  Its current-owner gate `.tmp/self_hosted/nominal-constructor-field-identity.EVfqRR`
  executes the same 31 C, 31 LLVM and eight source checks successfully without
  the held lifetime sources. Field-write gate `nominal-field-write-fact.uBThre`
  executes all 13 checks on C and LLVM. Native warnings remain observed.
- Focused constructor structural inventory uses the actual component checker
  definitions/call block: five cap requests, five function extractions and
  four reuses pass, including verdict's existing 600 cap. It is not full
  inventory. A first attempt incorrectly queried the responsibility cap table
  for verdict; that table correctly refused the absent row. The focused gate
  then consumed verdict's actual regular cap declaration from the caller.
- Full component inventory stopped at the 60-second budget twice (status124),
  once alongside native compilation and once serially. The checker mechanics
  and reached order/signature checks passed; unfinished inventory is not green.
  No cap/assertion failure was observed before these budget exits.
- `source_size_count.sgn7740d`: 31 existing lexical/CLI tests pass, including
  600 code records plus comments, 601 refusal, blank records and missing input.

Native harness success is not installed/Pergyra-built evidence or a fixed point.
The new native owner gate is a dependency of the existing collection semantic
Makefile target, which is already called by the Linux push step owner.

Baseline CI 36899513087 at exact base 53168fe9 completed with failure.
Build-linux job 110512829220 failed only push step 19/57:
one_mir_string_case_math_projection.sh counted the unchanged signature owner
with wc -l as 121/120. Its first line is comment-only; the existing lexical
counter measures 120/120 and still includes both blank records. No cap/source
change is warranted. This baseline result is not current-slice CI success.

Bounded counter repair card: existing lexical metric is the fact owner;
step19's projection cap loop is the last consumer. Replace only that reached
raw wc read, keep the same cap, and fail closed if measurement fails. Do not
remove comments/blanks, raise a cap, or stage the other dirty size gates.
Verification is the existing lexical/CLI tests plus the unchanged installed
String case/math projection gate. This is CI maintenance, not a lifetime rung.

## Parallel review and exact held boundary

Three read-only scopes reviewed control-flow/call-point proof, producer inout
effects, and CI/carrier negatives. Root alone integrated source/test changes.
Their findings were independently checked where this slice consumes them.

The final allocation review confirms removal of the new field-ID vector, not
cleanup of all existing call/literal views. It also identifies a remaining
cost: the source-signature owner performs a linear declared-function lookup
for each matched nominal, reached again by Core after the call-tree check.
This added O(nominal calls * function rows) owner read has not been measured
at compiler scale. No cache, index or general query track was added here.

The remaining BLOCKED rung facts are unchanged: Empty/Owned-preserving/noescape
producer effects, constructor/result event-time field state and descriptor
authority, and conditional inout-field retirement with exact writeback.
Statement inventory order and terminal local states are not call-point proofs.
Source typed block child order exists; pre-MIR lifetime state/join does not.
The existing collection owners must carry those facts before table Release can
be admitted. Preserve all cleanup consumers and pair empty/owned/FromArtifact
normal paths with borrowed, wrong-field writeback and repeated-release cases.

Registry status remains CLOSED=70 / BRIDGE=23 / ACTIVE=2. This slice closes no
whole row and replaces no C-owned compiler path. Unrelated dirty work and the
over-refusing lifetime candidate remain uncommitted and uninstalled.

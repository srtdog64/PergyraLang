# Owned sequence named value-boundary parity

Status: `LOCAL GREEN — PUBLICATION PENDING`

Exact base: `c3fc46b003d7bb60f371e311b38a0d757672a64c` on
`origin/main`.

## Objective card

- Objective: make the production self-host source-to-MIR entrypoint reject an
  unnamed function-returned `List<T>` or `Queue<T>` passed through an `own`,
  `ref`, or `inout` value boundary before MIR publication, matching the native
  borrow-tracked owned-sequence rule.
- Priority order: preserve native semantic behavior; extend the existing
  canonical sequence-shape owner; reject before MIR; retain named and
  default-mode List/Queue values; preserve the native fresh `ListNew()` and
  `QueueNew()` ownership-transfer exception; avoid a boundary-local type-name
  allowlist.
- Fact owner: `array_type_shape_owner.pgy` owns canonical Array/List/Queue
  owned-sequence shape. The named-boundary verdict consumes that shape with
  admitted parameter mode, expression place, and canonical contextual-builtin
  signature facts.
- Last legitimate consumer: `SemanticAstBodyTypeBundle` admission in
  `DriverSourceMirProjectionFromAdmittedRequest`, before verified MIR can be
  published.
- Forbidden fallback: exact List/Queue equality in the verdict, substring-only
  classification, treating all `SemanticSequenceElementType` values as owned
  and thereby widening to Slice, accepting function-returned owned sequences
  and relying on a backend, rejecting named/default values, or rejecting a
  contextually typed fresh List/Queue constructor once that separate typing
  bridge reaches this verdict.
- Verification gate: native and installed self-host reject function-returned
  `List<Int>` and `Queue<Int>` without C/MIR artifacts; named own and unnamed
  default-mode controls remain admitted; native fresh List/Queue constructors
  remain admitted; structural ratchets require canonical shape and builtin
  owners and prohibit the old Array-only or local allowlist path.

## Reproduced executable gap

- Native rejects `ReleaseOwnedInts(BuildOwnedInts())` and
  `ReleaseOwnedQueue(BuildOwnedQueue())` with the named-variable diagnostic and
  publishes no C.
- Installed self-host exits zero and publishes verified MIR artifacts of 8,164
  and 8,196 bytes respectively.
- Named `own` and unnamed default-mode List/Queue function results are admitted
  by native and installed self-host and publish artifacts.
- Native admits direct `ListNew()` and `QueueNew()` into the matching `own`
  boundary. Installed self-host currently fails those earlier at contextual
  builtin typing (`List<Unknown>` / `Queue<Unknown>`); this rung does not repair
  that separate bridge, but it must not add a second named-boundary rejection.
- A Slice member-call probe fails in installed self-host at incomplete carried
  call-target rows, so Slice is outside this edit scope.

## Edit scope and integration

- Allowed implementation: the existing canonical sequence shape owner and the
  existing semantic named-boundary verdict owner.
- Allowed tests: permanent List/Queue negative and named/default controls, one
  consolidated parity gate, and the minimum Make/owner/registry references.
- Allowed coordination/evidence: this directive, current collaboration and
  handoff cards, one dated audit, the SoT registry gate reference, and the
  generated language-word inventory only through its official generator.
- Forbidden overlap: Slice call-target repair, ListNew/QueueNew contextual
  typing, Set/Map/Option/Result/tuple/nominal/resource classification, backend
  List/Queue ABI or lowering, and unrelated BRIDGE rows.
- Integration owner: the primary task at this exact base.
- Integration gate:
  `tests/self_hosted/parity/direct_mir_owned_sequence_named_value_boundary_owner.sh`.

This directive is a temporary coordination artifact. It does not own semantic
status, registry census, project percentage, remote publication, or a successor
implementation rung.

## Local result

- The canonical shape owner now distinguishes owned Array/List/Queue from the
  broader sequence fact that also includes Slice.
- The named-boundary verdict consumes that owned shape and preserves matching
  fresh List/Queue construction only through the contextual builtin signature
  owner.
- Current-source DRV-2, the consolidated native/self-host gate, the existing
  Array regression, native production-bootstrap C emission, collection call
  protocol, full component inventory, hard contract, SoT edge/live adequacy
  with declared prover skip, single-owner, likeness, language inventory,
  documentation, and progress checks are locally green. Publication remains
  pending.
- The focused List/Queue body-parity C leg passes. The LLVM test-driver build
  remains red at the pre-existing
  `driver_source_llvm_intent_execution_owner.pgy:29` contextual Option gap;
  this directive does not reopen that separate source-to-LLVM intent bridge.

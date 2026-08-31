# Semantic Generic-Specialization Pressure — 2026-08-31

Status: `IMPLEMENTATION CANDIDATE — LOCAL FIXED POINT GREEN`

Exact base: `c5c50c4968418009c36cd33ed17c8e0ad937bcde` on
`origin/main`.

This directive coordinates one bounded compiler-scale performance rung. It
does not change language semantics, move the generic-specialization fact owner,
add a cache/query authority, claim SoT closure, or relax the fixed point.

## Objective card

- Objective: reduce the reached full-driver cost inside
  `SemanticAstGenericSpecializationFactsFromAdmittedBody` by identifying and
  deleting one repeated owned operation while preserving semantic output for
  unchanged fixtures and byte-identical candidate gen2/gen3 C.
- Priority order: semantic call identity and fact completeness; fail-closed
  unresolved-generic diagnostics; stable owner and consumer contract; exact
  output identity; removal of one measured repeated operation; then wall time
  and memory.
- Production execution boundary: both the typed-source producer and the
  MIR-consumer reconstruction reach semantic body-type assembly. Fresh bounded
  pressure shards measured the same generic-specialization owner at about
  52-53 seconds in each route.
- Fact owner: `SemanticAstGenericSpecializationFactsFromAdmittedBody` owns the
  call-node, stable source-call identity, signature, and concrete-actual rows.
  Instrumentation may observe this owner but must not create a second fact
  table or lookup authority.
- Last legitimate consumers: `SemanticAstBodyTypeBundle` admits the rows;
  self-MIR lowering serializes them; MIR reconstruction and codegen views
  consume their stable identities and concrete actuals.
- Candidate repeated operations to falsify: repeated linear membership lookup
  over `call_node_ids`, repeated traversal of the same expression-graph node,
  and repeated per-surface environment seeding. None is accepted as the cause
  until owner-local counts and timings distinguish it. A fourth reached
  candidate is node-ID-to-surface lookup hidden beneath each root query.
- Forbidden fallback: a general cache/query engine; source-text or spelling
  lookup beside stable call identity; skipping explicit or unresolved generic
  validation; a second fact owner; input shrinking; timeout/memory increases;
  parallel compiler-scale emission; or accepting byte-different MIR/C.
- Verification gate: owner-local count/timing evidence first; then the existing
  semantic environment/function-table structural gates and generic
  specialization parity/negative gates. The producer and consumer pressure
  shards must use one candidate full-driver input; candidate gen2 and gen3 C
  must be byte-identical. The pre-change hashes below identify the baseline,
  not an impossible requirement that a self-host compiler source edit leave
  its own full-driver MIR unchanged.

## Fresh falsifier

- `self-host-driver-full-mir-seed-shard` completed in 218,871ms, peaked at
  1.861GiB working set / 1.994GiB private memory, and recorded 22,287 stages.
  Generic-specialization assembly was its largest named gap at 52,155ms.
- Its 275,728,528-byte focused MIR has SHA-256
  `12050fdf7917e0a6f50d187f2bdb5f6c56b1d0900e2a7f3a8acb1b13e512842c`
  and is byte-equal to the full producer artifact.
- `self-host-driver-full-mir-consumer-shard` completed in 226,338ms, peaked at
  1.832GiB working set / 1.989GiB private memory, and recorded 4,352 stages.
  Reconstructed generic-specialization assembly was its largest named gap at
  53,156ms.
- Its 11,440,003-byte focused C output has SHA-256
  `2ec56d6d34c9d4ababcb22a9aca0d1280312c91b745c855512d72aac14f1cc13`
  and is byte-equal to the full consumer artifact. The consumer input is the
  same byte/hash-identical MIR measured by the producer route.
- The whole fixed-point wrapper previously took 2,369,221ms and peaked at
  2.425GiB working set / 2.641GiB private memory. This card narrows that broad
  pressure to one reached owner rather than inferring a compiler-wide design.

## Repeated-operation diagnosis

- A temporary diagnostic clone of the already-generated seed C observed the
  owner without changing repository source. The full driver had 766,885 graph
  nodes, 126,513 expression surfaces, 90,157 environment seeds, and zero
  generic-specialization rows.
- `SemanticAstGenericSpecializationIndexForCall` was invoked 409,688 times but
  performed exactly zero comparisons because the fact set was empty. The
  linear generic-membership hypothesis is therefore false. The graph traversal
  visited exactly 766,885 nodes, so duplicate graph traversal is also false.
- The first graph/environment phase took 34,522ms and unresolved validation
  took 8,434ms. Inspection reached
  `SemanticAstExpressionGraphRootForNode`: every caller supplied a source node
  while that view rescanned up to all 126,513 sorted surface node IDs. The
  generic owner invokes it for graph presence and again for each lane.
- A temporary generated-C lower-bound experiment changed only that lookup. Its
  graph/environment phase fell to 18,361ms while unresolved validation stayed
  at 8,469ms. Its pre-source-change MIR remained byte-identical to the baseline,
  proving the measured saving came from lookup work rather than fact changes.

## Local implementation candidate

- `SemanticAstExpressionGraphRootForNode` now performs a lower-bound search on
  the strictly increasing surface `node_ids` already guaranteed by
  `SemanticAstExpressionSurfaceBorrowReady`. It preserves the same missing
  node, invalid lane, missing root, and bounds failures. The view owner remains
  exactly at its existing 100-line cap.
- The component ratchet requires the lower-bound middle step and forbids the
  former `i = 0` linear loop inside this function. No cache, index table,
  second owner, or new semantic fact was added.
- Current Pergyra-built codegen emitted the changed source successfully. The
  source candidate producer completed in 139,638ms with 1.860GiB peak working
  set / 1.987GiB private memory; its generic stage fell from 52,155ms to
  18,593ms. Its 275,738,488-byte MIR has SHA-256
  `97af2abe3a7fcc92f13a4ad0959e1db6f0a4666d2e9313a8314bc2baed594a73`.
- The candidate consumer completed in 146,267ms with 1.836GiB working set /
  1.988GiB private memory; its reconstructed generic stage fell from 53,156ms
  to 18,734ms. The gen2 C artifact is 11,440,236 bytes.
- A binary compiled from that gen2 C emitted gen3 in 162,486ms; its generic
  stage was 18,907ms. Gen2 and gen3 are byte-identical (`cmp=0`), each 172,787
  lines / 11,440,236 bytes with SHA-256
  `e4440e785fdd4f8d4152fb21dd9752704ef40e01f16482d29d2552f348e08cf7`.
- Candidate execution reproduced the four generic-return output lines exactly.
  Callable resolution succeeded; target, nested, and explicit mismatch modes
  returned their exact expected diagnostics. The identity-epoch execution and
  missing/crossed binding plus bad-ordinal negatives passed. A direct explicit
  generic mismatch returned `call_arg_type_mismatch` and published no artifact.
- Semantic environment lifetime, shared function-table, agent boundary, and
  documentation-quality gates are green. The broad component gate was stopped
  after it exceeded the 60-second static budget; it had reached and passed the
  source-MIR action ratchet but is not claimed green locally. Exact push CI is
  the next broad integration falsifier.

## Scope and budget

- Allowed edits: owner-local pressure observation; the one semantic owner and
  responsibility-named data it exclusively owns if the measurement proves the
  repeated operation; focused structural/parity gates; and current
  coordination/handoff documents.
- Integration owner: the primary task owns measurement, implementation,
  integration, commit, push, and CI interpretation. No parallel implementation
  scope is open on this semantic owner.
- Allowed commands: static owner gates under 60 seconds; focused generic parity
  under five minutes; one producer and one consumer pressure shard under the
  existing 300-second / 3072MiB limits; exact artifact hashing; and one exact
  CI run only after the bounded slice is stable.
- Out of scope: unrelated SoT rows, parser/runtime size debt, new language
  syntax, generic ABI redesign, cache/query architecture, fixture expansion,
  test sharding, and binary hardening.
- Protected unrelated untracked paths remain outside inspection, edit, and
  staging: `docs/compiler_architectures/`, `pgy-80135c2c/`, and
  `pgy-91d769ec/`.

## Output classification

Counts, timings, and pressure artifacts are observations. A source change is
an implementation candidate only after those observations identify one
repeated operation and the negative gates remain intact. This rung changes
neither 88 authorities / 183 carriers / `CLOSED=55 BRIDGE=32 ACTIVE=1` nor the
83% project forecast by itself.

# Delivered ABI / ArrayDrop CI integration repair

Base: `b5601b8b1359318630fe3943584197f39a137232`.
This is delivery integration, not a successor compiler rung, registry closure,
new C substitution, fixed point or aggregate-field lifetime delivery.
Objective and independent review boundaries are in
`docs/agent_work_directives/abi_delivery_ci_integration_2026-10-02.md`.
Root is the sole editor/finalizer; reviewers did not edit production files.

## Observed CI failures

Run 36877097428 ended `failure`. Its self-host-contracts job 110437455907
failed runtime object compilation and two source-inventory checks that still
expected pre-projection SliceCopy/owned-parameter LLVM ABI text. The public
ArrayDrop structural owner check had passed before the runtime build failed.

The Linux build and backend comparison shards 6/11 instead timed out during
dependency installation at ten minutes. Bootstrap reached gen2 preflight,
then was cancelled during gen3 emission at its sixty-minute execution limit.
Those are separate unresolved infrastructure/execution-budget observations,
not failures proved repaired by this change.

An actual Ubuntu-E-WSL clang reproduction in
`.tmp/self_hosted/aggregate-formal-lifetime.SibVu8/linux-runtime.cOD0Wl/`
refused the old strict-C11 command with undeclared `CLOCK_MONOTONIC`.
The Makefile-aligned Linux feature-macro profile produced a nonempty runtime
object with unchanged source/profile hashes. This is one Linux runtime TU,
not whole Linux CI or a Windows-to-Linux success inference.

## Changed boundaries

- One linked-runtime compile-profile owner carries the existing platform
  contract to four executable gates. Unknown platform and failed platform
  discovery refuse; compile failure logs remain visible. Existing positive
  and negative matrices are unchanged.
- SliceCopy and owned-ArrayString parameter inventories consume projected
  aggregate type/alignment facts. Old literal layout and external C
  aggregate-return ABI ratchets remain negative checks.
- The semantic C dispatcher delegates complete array storage-release
  materialization to `expr_array_storage_release_emit_owner.pgy`.
  Public plain-value release and compiler retirement keep their existing
  semantic permission owners; the emitter cannot grant element lifetime.
  The old retirement definition/inline public-drop body are deleted.
- Exact ArrayDrop/ArrayLength value signature projection has one named
  consumer owner. The caller dispatches only those two names, preserving
  canonical registry/kind/ABI identity and unsupported ArrayLength fallthrough
  to the existing argument-prefix refusal. It is not an extra hot-path query
  for every builtin.
- `ast_expression_environment_owner.pgy` explicitly imports `env_owner.pgy`
  for its real `EnvContainsName` call. This fixes the reproduced FromArtifact
  source import failure, not the missing field lifetime proof.
- Array-index inventory follows the new signature owner; the retirement
  probe hashes the actual new materializer and uses the native transpiler's
  reserved `pgy_u_` user-callable symbols in its watched C harness.
  Unique evidence directories replace reused directory deletion in the
  reached SliceCopy/scalar-index gates.

No existing cap was raised. Comment-excluded counts, including blanks:
semantic C dispatcher 572/600, storage-release owner 45/50, builtin signature
dispatcher 217/230 and array-value signature owner 31/40. The runtime header
owner remains 19/20; the new linked-runtime profile is 15/20.

## Executed evidence

Evidence root: `.tmp/self_hosted/abi-ci-delivery.bxSxwK/`.
Native developer harnesses execute current Pergyra owners; they are not a
Pergyra-built replacement driver or installed/self-host receipt.

- Final `current-owner-parity.0c6nZv`: 72 byte-equal signature/prefix rows;
  ten byte-equal C projections and ten normal executions; six identical
  diagnostic refusals preserving prior output. Source/probe/installed hashes
  were rechecked. Only normal positive programs were executed.
- Zero-arity current materializer unit: exit 1 with exact one-argument refusal
  before any operand/type read; owner/probe hashes unchanged.
- Actual body-retirement/Bool-runtime probe `body_bundle_typed_retirement.G4oqRh`:
  twenty distinct column backings freed once, shared zone String not freed,
  concrete Bool layout and actual retirement rewrite preserved; populated,
  empty, null and allocated-empty drop/reset observations passed.
- Stable installed scalar-index C/LLVM parity, out-of-bounds panic and ABI
  negatives passed. It validates the anchor migration without claiming a new
  installed implementation.
- Stable installed SliceCopy bridge `slice_copy_semantic_bridge.KqVJRh`,
  public ArrayDrop `public-array-drop.6NH04g`, ArrayString ABI
  `array-string-layout-closure.KjHOr3`, String index/mutation
  `one-mir-string-array-mutation.hbKBX8` and receiver-use
  `array-mutation-receiver-use.DSxtCv` passed their existing focused matrices.
- Platform unit: five profile mappings and two refusals passed; not five
  actual platform builds.
- Environment import regression `table-source-c.0ciMok`: three installed MIR
  producers and six native/public source-C executions passed for empty,
  owned and real FromArtifact tables. This is not field-candidate admission.
- Final isolated staged component inventory: exit 0; 2,567 line-cap requests,
  1,057 function extractions and 713 reuses. The final log is
  `staged-inventory-final-provisioned.log`.
  Nineteen staged files were byte-identical to their index blobs before the
  final documentation refresh. The snapshot excludes the held lifetime sources, five new
  fixtures and unrelated dirty changes. It is source inventory only.

Two short-budget component runs returned 1 without an owned diagnostic;
bounded tracing did not reproduce an assertion failure. An initial snapshot
attempt mixed the cwd's cap list with an unpatched archive and is invalid
evidence. The corrected index-equal snapshot first refused a Makefile dry run
because its ignored `build/` directory did not exist; an empty build directory
was then provisioned before the final integration run. None is a green gate.
The first watched retirement harness failed at obsolete bare native symbols;
the corrected observed run above passed. Native unreachable-code and LLVM
target-triple warnings remain; no full matrix/fixed-point pass is claimed.

## Held ownership candidate and next falsifier

The latest developer-only `matrix.WGSsq3` has seven exact
`borrow_boundary_escape` negatives and five normal controls, but all three
real table-release positives still refuse. No unsafe accepted input was run.
The candidate is neither installed nor staged in this delivery.

Exact missing facts: producer helper inout Empty/Owned preservation and
no-escape effects; constructor/result carriage of each Array element state
by canonical field identity; exact inout release authority and retired field
writeback. Readiness, field spelling, descriptor type and parameter mode are
not proofs. Existing collection verdict/member/transition/receipt owners
remain authoritative. The last cleanup consumers follow initializer, MIR
projection and source-C codegen materialization, respectively.

Next common falsifier: empty/owned direct table construction must retain
normal release while `callable_table_borrowed_negative` refuses under the
same type/inout/shape. Real FromArtifact additionally needs producer/result
effect carriage. Preserve its genuine retirement; no name-based exemption.
The separate public LLVM empty-table expression frontier also still refuses.

SoT stays `CLOSED=70 BRIDGE=23 ACTIVE=2`; this repair closes zero whole rows
and replaces zero C-owned compiler paths. The exact missing-fact/owner/last-
consumer/falsifier BLOCKED continuation card remains in force before another
SoT-only successor commit. The goal itself is not marked blocked or complete.
Alrescha stays independent. No TypeSafe/Jev/compiler integration was added.

Installed driver remains `707DCD40049A1697A5827B2A7C8D3CF509573AA3C0031F2EEE338C9FA0D78EC7`;
manifest remains `0A83B0DB5EFE3C00C6D9413C63045C4B17AFF079781213B280442C588E5A9C19`;
native remains `A5DFE2CB3258216AB546EA39EEF65839D45100950D7DF7295D756DA9F72B3936`.
No installed artifact was replaced by this integration session. Current-commit
CI must be observed after push; the preceding run is not its acceptance proof.

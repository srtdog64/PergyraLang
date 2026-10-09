# Ownership cutover prerequisite checkpoint and contract receipt

Status: `BASELINE FIXED / BOUNDED IMPORTING PROOFS; FULL P1 OPEN`.
Date: 2026-10-09 KST. This receipt is navigation/evidence, not semantic
authority, implementation approval or compiler/SoT closure.

## Scope

The user approved the four prerequisite items and local checkpoint, then
clarified that the ABI vocabulary must include multi-output/view facts and
production C/LLVM consumer migration stays in the later transition. All other
shared writers were confirmed stopped for commit-through-baseline execution.
The current slice fixes the baseline, coupled contracts and bounded importing
proofs. It does not silently start P2-P7 or claim whole P1 closure.

## Local baseline

- Base: `3658548d24bca3d721e4f1974ac7a10da99f7aa8`.
- Checkpoint: `a75da80435e0d051f71cfacf76d44ea825ed87d7`, on main, local only.
- Tree: `513cc18b1785c4f623045a38463a2b5cbdd6f612`.
- Captures 456 changed/new gate-input paths, including existing shared WIP.
  These are preserved inputs, not feature-completion claims. Three document
  EOF whitespace defects were corrected before the commit; no new compiler,
  runtime or proof-definition edit was made to construct the checkpoint.
- Frozen execution: tracked inputs diff 0 before and after the baseline
  gates; only pre-existing `gmon.out` untracked. `.tmp`/profiling outputs,
  secrets, proof build artifacts and installed binaries are not committed.
- The previously unpushed three commits plus this checkpoint are future
  candidate ancestors. No push was made; publication requires separate approval.

Observed post-commit gates:

| Gate | Result | Scope |
|---|---|---|
| `tests/ownership_cleanup_smoke.sh` | PASS | Existing five-module kernel/extraction; decision/refusal/summary, 43 composition, 37 readonly, nine place/view and eight exit controls; model only |
| `tests/coq_kernel_check.sh` | PASS | 72 modules plus approval export/binding consumer; two approved Slot abstractions, no admits/unsafe kernel features |
| `tests/documentation_quality_smoke.sh` | PASS | Documentation/static surfaces, not compiler behavior |
| checkpoint text/drift checks and staged `git diff --check` | PASS | Reviewed input set, strict UTF-8, credential markers and exact frozen paths |

Existing Rocq masking/load-path/nested-induction warnings were not suppressed.
The old doc 19 CRLF-to-LF normalization warning was retained under Git policy.
No new native/C/LLVM/DRV-2, sanitizer, full compiler-scale memory,
installed-driver or remote CI run is claimed. Full P0 records those outcomes
separately, including reds/omissions; a checkpoint cannot replace them.

Baseline artifact identities (unchanged installed/local files):

- Windows compiler: `f6559da94876c93a2e7303866429ef31ef284b1f29ca3d68f67ff5e710cbc1c9`.
- Windows self-driver: `707dcd40049a1697a5827b2a7c8d3cf509573aa3c0031f2eee338c9fa0d78ec7`.
- WSL compiler file: `9e2048c923fb4234880d21ae6efd858fe0493ae58083f9c373dc84c0c0532449`.
- `bin/pgy-self-driver` is absent; Windows parity is not Linux installed parity.

Operational repair: the old zero-byte `.git/index.lock` prevented staging.
Windows/WSL checks found no active Git process. It was moved, not discarded,
to `.tmp/ownership-preflight-2026-10-09/stale-index-lock-2026-10-08` and can
be recovered. No history reset or worktree/stash operation was used.

## Coupled design decisions

Semantic owner: doc 27 §5.10.1–3. The target `OwnershipCleanCallContract` joins
resolved call identity, the existing ABI owners and semantic origin/lifetime
facts, and includes every recovery output and the ordinary outcome. Consumers
cannot infer these columns from a target symbol or ABI layout. Production
fact issuance/carrier registration and consumer migration are still P3/P4.

Multi-inout uses existing `SPack -> SCallIO -> SUnpack`, with a fresh private
bundle and outcome packet. Unpack precedes handled-error dispatch; internal
early returns reach one recovery epilogue. Current `SCallIO` and exits do not
support throwing through the call before unpack. Also, exact repeated core
variables are already refused by `SPack`'s `nodupb`, not silently copied;
the normalizer is the first alias defense before bundling can hide actual
identities. Source alias/place admission is still required. Pointer/ghost-bundle
refinement and physical allocation cost are separate P1/P7 obligations.

Slice reuses the lease/pin lifetime vocabulary, not the readonly-copy
substitution. In-range write-through is separate from structural mutation.
The lease model supplies no write/exclusivity permission, and its graph
handle cannot be replaced by an AST or snapshot ID without a checked binding.
Static lifetime issuance is the default; runtime `{data, length}` is unchanged.
Live old String/payload borrows must also survive or block replacement.

## Importing evidence after the baseline

The fixed edit inventory was recorded before proof source edits. Two new
supplements import existing owners; no existing cleanup or teardown core was
changed. No compiler/runtime/backend source or descriptor was changed.

- `OwnershipCleanCallRecovery.v`: ordered bundle/call/unpack source execution
  under the actual adapter body's execution/shape premises. Alias admission
  and private-name freshness against the complete supplied caller scope
  precede packing; one `RecoveryAdapter` has operational catch semantics,
  borrowed-parameter support, a body-level escaping-loop-control checker and
  an independent tagged value/error packet. Arity/tag decoding refuses bad
  outputs; value-level handler dispatch observes restored inouts.
- `OwnershipCleanViews.v`: a dynamic ghost oracle for current owned-footprint/
  range evidence, not static liveness discharge. Scalar write-through uses the
  same heap and preserves INV and source CORR. Listed structural refusal,
  guarded backing drop refining `TE_Drop`, and ended static-ticket exclusion
  throughout issuance/end schedules. Metadata end frees no backing. Graph
  lease kinds are reused without relabelling a source ID as a graph handle.
- `OwnershipCutoverPreflightAudit.v`: binds exact propositions and checks
  positive/refusal behavior. A two-inout call plus independent result and a
  readonly argument executes with zero abstract copies and an empty final heap.
  All three continuing outcomes execute packaging; one compiled branch adapter
  handles both return/error inputs. A scalar write reads back through the
  same owner; stale/readonly/out-of-range/ended evidence refuses. Live borrow
  and pin refuse backing drop; after end the existing core drop is admitted.

Final fresh focused kernel gate observed PASS on the identified supplement
snapshot below: seven modules, no assumptions/admits/unsafe features. The
full formal/kernel results below were rerun after the Low repairs; earlier
intermediate compiles are not substituted for those results.

## Remaining proof/implementation boundary

The baseline method and contract choices are fixed, and the importing
recovery/scalar-view propositions above have compiled. Remaining P1 work:
arbitrary exiting callee lowering into the normal-only core call table;
actual caller decoder/handler lowering, source expression/index ordering,
complete/current caller-scope issuance, language type-schema validation and
physical rooted-place disjointness;
pointer/ghost-bundle representation/cost refinement; full static liveness
issuance, single-current-state/snapshot binding, whole-texec view frame and
complete operation effect sets; general owning-payload glue,
mutable exclusivity and graph/backing binding. These are not replaced by a
predicate or conditional theorem. Production fact registration, native/
self-host/C/LLVM consumer migration and all eight full landing gates remain
later work. Full P0 compiler-scale memory/DRV-2/CI remains unmeasured/unrun.

Logs/manifests: `.tmp/ownership-preflight-2026-10-09/`. The earlier two Claude
reviews concern the prior contradiction repair, not approval of this new
contract text.

## Actual Claude co-review and repair

Read-only installed CLI review completed successfully: session
`18d2c892-448d-4a34-b23e-82539bd6abc5`, 438606 ms, Read/Grep/Glob only,
no spawned agents/denied permissions. Hooks/custom skills and MCP were disabled;
no model override. Raw result: `.tmp/ownership-preflight-2026-10-09/claude-preflight-review.json`.
The reviewer observed intermediate edits and reread the final version it saw;
that review is not approval of subsequent repairs or production closure.

- High alias hole: a readonly actual equal to an inout could be hidden by the
  private bundle name and silently copied. Repaired with the admitted
  normalizer, inout/argument/private-name refusals and no-preservation-copy
  proposition. The independent falsifier is now a refusal.
- Medium catch/decode scope: replaced the meta-only catch claim with one
  operational adapter, kept the post-exit lemma honestly named, and added
  shape/tag decoder and restoration-before-handler proposition/controls.
- Medium borrowed parameters: generalized recovery B and refused borrowed
  output/private names; the normal core call witness includes an actual readonly
  argument, and borrowed branch-adapter admission is checked.
- Medium loop escape: added the body scope checker, its execution theorem and
  actual body-level break/continue refusal controls (local loop control remains
  allowed).
- Medium output shape: added executable arity/packet/tag refusal. Full language
  type-schema admission remains OPEN rather than being inferred from SVal.
- Medium source correspondence: added the scalar source backing/CORR frame
  proposition and an exact typed consumer, not just heap equality.
- Medium static-currentness overclaim: reclassified this as a dynamic ghost
  oracle. Model block IDs are not reused; old evidence-state copies and direct
  base texec operations are not made safe by it. Static issuer, state/snapshot
  binding and whole-instruction frame remain explicit OPEN obligations.

Second read-only review completed successfully: session
`da2c78c1-1637-4c46-b1d0-e98bf662a80b`, 182925 ms, same restricted CLI
configuration, no spawned agents/denied permissions. Raw result:
`.tmp/ownership-preflight-2026-10-09/claude-preflight-rereview.json`.
It found no remaining High or new Medium inside the explicitly bounded
model scope. This is a source review, not a kernel run or production approval.
Its fixed snapshot was CallRecovery `94590969...`, Views `8bfd2b6d...` and
Audit `4348604e...`; it predates the Low-only repairs below.

GPT subsequently repaired the four Low follow-ups and independently compiled
the new supplement and audit. No third Claude approval is claimed:

- Private-name freshness: the normalizer now requires the complete pre-call
  scope, checks actual membership, and refuses bundle/packet collisions with
  other locals, not only call operands. Existing-local and missing-actual
  falsifiers refuse. Completeness/currentness of the production scope issuer
  is still OPEN; supplying an incomplete list is not evidence of freshness.
- Exact consumers: the audit now binds the admitted source-CORR view frame,
  admitted storage/INV frame, generic alias refusal and preservation-filter
  propositions by their complete types.
- Copy-bound connection: `admitted_first_pack_has_no_copies` consumes an
  actual `elab` result for the first pack at `bundle :: args`, with the admitted
  alias/borrow premises. Its exact consumer prevents replacement with a mere
  boolean or unrelated theorem. Whole-callee copies, physical place aliasing
  and production pointer/bundle allocation erasure remain separate.
- Alias-defense wording: doc 27 and this receipt now name normalizer admission
  as the first defense; the existing `SPack` duplicate check is not a substitute.

## Final importing snapshot and observed integration

HEAD remains `a75da80435e0d051f71cfacf76d44ea825ed87d7`. These are post-checkpoint
preflight changes, not a second committed/clean-SHA candidate or a production
cutover landing. Proof and executable gate sources did not drift during the
final focused/full runs. Later receipt/handoff edits report the observed
results; they are not new semantic facts or proof-source revisions.

| Input | SHA-256 |
|---|---|
| `OwnershipCleanCallRecovery.v` | `6b82ea274f96e45aa6c8282627f94b41d2b3c7d48bf07bfecfe34e18db143878` |
| `OwnershipCleanViews.v` | `8bfd2b6d4abc5d5ebba6d1cdb022296ce13cdaf975ea463fda664bb5568b9c5f` |
| `OwnershipCutoverPreflightAudit.v` | `fa8b1e6046ac04addad2352afa0c8eeb7f76bbac0c1791d6137b3005d45d25af` |
| focused preflight smoke | `c04454e6fc671e53b3d261e813f8ef623b3c06ef7acc8b75188fe5df4d6f63ed` |
| kernel gate | `2beffa44211c2e279e226176afa502afb7d8a21b6e45cf3d228ae7201b73ab32` |
| formal semantics gate | `221d4375ad431570b70801ce4a6a0222f86ac9b194a8c14bfc7efc0ef41aea7c` |
| doc 27 coupled contract | `a0841cac5197d970ce1ffed7f16f1bae161661cc8d36e7a596304f5ede5e4e0b` |

Rocq 9.3.0 / Stdlib 9.2.0 via the admitted project wrapper:

| Gate | Observed result | Exact scope |
|---|---|---|
| focused preflight | PASS | Seven modules freshly compiled/kernel-checked, zero assumptions; typed and executable model consumers |
| formal semantics + delegated full kernel | PASS | 67 proof-owner modules plus eight permanent consumers (75), plus approval export/binding consumer; only `SlotCalculus.verify_token` and `SlotCalculus.MaxSlotId` approved assumptions |
| SoT registry structural gate | PASS | 95 authorities / 201 carriers; CLOSED 70, BRIDGE 23, ACTIVE 2; statuses unchanged, not semantic closure |
| shell syntax + `git diff --check` | PASS | Three affected gate scripts and scoped text; compiler/runtime source diff and staged paths are empty |
| documentation quality + strict UTF-8 | PASS | Repository documentation surface gate and all 15 scoped inputs; not compiler behavior |
| final kernel source-drift check | PASS | Every one of the 75 logged module SHA-256 values equals the final working source |

Logs: `preflight-low-final-focused.log`, `preflight-low-final-formal.log`,
`preflight-low-final-sot.log`, `preflight-low-final-docs.log` and
`preflight-low-final-hashes.json` under
`.tmp/ownership-preflight-2026-10-09/`. Fresh snapshots and the assumption
summary are in the kernel logs. Existing masking, nested-induction,
deprecated-notation and approval-load-path warnings remain visible.

The task leaves 15 scoped docs/proof/gate files uncommitted and the original
untracked `gmon.out` untouched; staged paths are empty. Compiler/runtime and
installed binaries are unchanged after the baseline. No source C/LLVM,
sanitizer, DRV-2, compiler-scale memory, installed-driver or remote CI result
was produced by this proof preflight. No push, GUI notification or whole-P1
closure was made.

Next falsifiers for the remaining agreed P1 chain: a real exiting callee in
the normal call table must recover every inout before the actual caller
handler runs; a live writable view across branch/loop/call uses must keep its
backing alive and refuse all conflicting structural instructions before any
destructive step. An incomplete/stale scope or lifetime snapshot must refuse.
The current conditional recovery and dynamic ghost checks do not discharge
those production/static refinements.

# Read-only alias elision: bounded proof and extraction

Status: IMPLEMENTATION COMPLETE (bounded local proof/extraction only);
production integration OPEN. Base:
`3658548d24bca3d721e4f1974ac7a10da99f7aa8`, preserved dirty main.
Core input f8ac63cf9a9a47389863761b3c84a49279b7e683f807956950844bc3d0d69d00;
composition input e12836730e707b7a6bf4e753cbf7480ccf9d9bf9c320315253b3282679f7c7a0.
The user requested all of the proposed read-only elision work and falsifiers.
This supplements the previous user-authorized Rocq slice, not a takeover of
the concurrently owned C3/C5 compiler implementation.

## Objective card and whole chain

- Objective: eliminate a nonescaping read-only local copy, preserving source
  observations and canonical ownership cleanup; prove a genuine allocation
  reduction on executable witnesses, not merely fewer syntax nodes.
- Priority: value semantics and root lifetime, explicit refusal, one admitted
  elaborator, extraction/negative evidence, then costs.
- Owner: OwnershipCleanCore owns SStmt/sexec/elab/texec and heap facts. A new
  imported proof module owns a checked local source rewrite only. No second
  heap, target machine, drop policy, RC/GC, pointer or user annotation.
- Chain: admitted local copy + its continuation -> readonly-region admission
  -> root-name substitution -> existing elab/normalize -> typed proof
  consumers -> extracted observer -> hash-bound receipt.
- Last consumer here: fresh extraction gate. Production entrypoint remains
  driver_app -> mir_lower -> SSA/use/cleanup/recompute in src/compiler/mir.c.
  C2 exists in doc 27 section 5; no pgy.mir.ownership.v1 pass/facts producer
  or named ownership-clean pass is reached by that production chain yet.
  Backends must not independently invent elision/glue. C3/C5 and physical
  C/LLVM/runtime refinement are separate required integration obligations.
- Forbidden: shallow shared owning descriptors; releasing the root before
  an alias read; rewriting a live-out or borrowed destination; writes,
  retention/consumption or unknown calls through the region; claiming every
  field projection/escaping closure is now covered; compiler/CI/SoT closure
  from formal evidence alone.

## Complete required changes and dependency order

1. Import the canonical source/target machines. Check a bounded readonly
   region: observations, pure definitions, copied field reads, sequences,
   branches and finite loop executions. Reject writes to alias/root, storage
   or consumption, calls and unsupported statements. A field read remains a
   fresh value copy, not a general borrowed member-path implementation.
2. Prove root substitution preserves source execution and all final values
   except the retired alias binding. Compose through an unchanged prefix.
   Consume canonical closed-program cleanup to prove optimized execution.
3. Prove a conservative static allocation certificate against texec's actual
   allocation frontier; do not sum both branch arms as runtime cost. Calls,
   loops and unequal arm costs remain explicitly unmeasured by this observer.
   Establish same trace/empty heap and one fewer allocation on both branch
   outcomes; test loop/name-shadowing/escape negatives separately.
4. Wire the new proof/rewriter to the existing extraction gate and exact
   proposition consumers. Preserve prior controls and fixed cost inputs.
5. Rerun the residue gate on the same complete source set using a native host
   shell if available; no timeout increase, smaller source set or silent skip.
6. Update doc 207, proof inventory, audit and this task's handoff card.

Independent edit scope: new imported readonly proof, G1 observer/gate/receipt,
proof inventory and this slice's docs. Canonical core, C2, production compiler,
runtime and other workers' files are read-only. No new worktree, delegation,
installation, staging, commit/push or messages in this proof slice.

Integration owner: this task. Gate: ownership_cleanup_smoke.sh (300 s), full
formal_semantics_smoke.sh (1800 s), residue/selftest (60 s each). Exact input
hashes bind receipts. Outputs are bounded implementation candidates until
the gates run; physical compilation cannot be reported from source rewriting.

## Observed completion receipt

OwnershipCleanReadOnly.v imports the unchanged core and composition owners.
Current supplement hash:
14b0122898faf83bffb13836087a6c6bbdd0bf97ef255d08e57fe28a5eb5d8bb.
It proves trace preservation, canonical closed cleanup and allocation-frontier
accounting, including two target executions with 3 -> 2 allocations and the
same trace/empty heap. A denied root mutation has a concrete 7-vs-9 semantic
counterexample. The wrapper requires original closed admission before any
elision; an unused alias cannot hide an undefined root. D1 remains outside
the abstract core and applies to actual remaining copies at integration.

Observed PASS: fresh extraction kernel (4 modules, zero axioms), 15 decision
+ 24 refusal + 40 sink/composition + 32 readonly controls, unchanged 30 cost
cases and two branch allocation witnesses. Full formal corpus: 60 modules
plus approval consumer, only the two approved SlotCalculus assumptions.
The same whole-source direction gate/selftest passes in native Git Bash
under the original 60-s budget, without changing the gate or input.

Receipts: .tmp/ownership-cleanup/readonly-{extraction-final,formal,direction}-2026-10-08.log
and readonly-cost-2026-10-08.json. Source drift checks bind eight inputs.
Doc 207 section 10, proof inventory, facade and dated audit now describe the
algorithm and limits. No new workspace/worktree, language syntax, runtime
pointer/loan, core-machine change, production mutation, install or Git write.

The user was asked nonblockingly whether to take over C3/C5; no reassignment
was received for this slice. Preserve the existing implementation split.
Remaining proof boundary: member-projection alias/places and early exits,
then admitted MIR use/lifetime/glue refinement and C/LLVM/self-host evidence.
This directive closes only its bounded proof/extraction checklist, not a SoT.

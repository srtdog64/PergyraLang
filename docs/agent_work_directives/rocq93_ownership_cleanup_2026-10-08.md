# Rocq 9.3 toolchain and ownership cleanup core

Status: IMPLEMENTATION COMPLETE for G0/G1 and the G2/G6 source rollback;
G3/G4/G5 are dependency-pending. Base:
3658548d24bca3d721e4f1974ac7a10da99f7aa8, entered with 480 preserved status entries.
The user requested stable Rocq 9.3.0, removal of obsolete/repeated proof paths,
an executable ownership-cleanup model, and ownership performance testing.
The existing compiler/DRV-2 implementation hold is not lifted by this scope.

## Shared objective card

- Objective: admit one stable proof toolchain and implement/test the cleanup
  obligation algorithm without claiming automatic compiler cleanup exists.
- Priority: semantic identity and preserved assumptions, fail-closed version
  and fact admission, executable negative evidence, reproducible cost, then
  patch size. No weaker safety condition or larger gate budget is permitted.
- Fact owners: one project Rocq toolchain owner for version/CLI admission;
  OwnershipCleanCore.elab for move/copy/drop elaboration; existing kernel gate
  for assumptions; existing certificate predicate for certificate admission.
- Last consumers: fresh corpus kernel check and assumption-export consumer;
  extracted cleanup checker/executable controls and performance receipts.
- Forbidden fallback: rc1 admitted as stable, Coq 8 fallback, cached/orphan .vo
  as current evidence, a second heap/alias authority, a handwritten substitute
  for the extracted algorithm, or model timings called compiler speedups.
- Falsifiers: wrong/missing prover or checker; stale proof artifacts; planted admission
  and API type drift; duplicate pack operands; self-consuming definitions/push;
  missing loop-head facts; missing
  source/backend refinement; superlinear repeated operation on fixed workloads.

## Whole-chain map and required change set

1. Installation/CI: official stable V9.3.0 is released; official Docker tag
   9.3 currently resolves to rc1. Install rocq-core/runtime 9.3.0 and independently
   versioned rocq-stdlib 9.2.0 through opam. Preserve the existing Coq install;
   use a project-named switch. Prover and stdlib version admission is owned once.
2. Proof source: migrate deprecated Coq.* imports to Stdlib.* mechanically;
   do not change theorem statements, approved SlotCalculus abstractions or
   existing negative fixtures to make a new prover green.
3. Compile/check: replace hand-written foundation order with actual dependency
   order; build fresh isolated copies and kernel-check only this run's modules.
   Delete duplicate CLI selection, source-directory artifact writes and orphan
   artifact deletion. Keep explicit proof registration and assumption checks.
4. Consumers: kernel/self-test, formal inventory, proof spine, focused machine/
   SoT/methodology gates, certificate extraction and multiplication proof benchmark must
   consume the same admitted toolchain. Preserve declared missing-prover skips
   only in existing non-proof runner contracts; wrong versions never become skips.
   The methodology consumer was found during the complete CLI census and is
   included in this same migration. CI and Platform full share one proof job;
   compiler-only jobs remove the obsolete apt Coq dependency.
   The CI profile's consumer contract remains an acceptance gate for the
   reusable proof job and the declared no-prover compiler-only lanes.
   Final consumer census also reaches beta_readiness_checklist_smoke: it still
   requires apt Coq in Platform full. Migrate that structural consumer to the
   reusable stable proof dependency and ratchet the obsolete apt Coq row;
   do not restore a second prover merely to satisfy the stale inventory.
5. Cleanup algorithm: use Claude's OwnershipCleanCore.elab as the sole model
   authority, per ownership_clean_work_split_2026-10-08.md G0/G1. Delete GPT's
   uncommitted OwnershipCleanup.v instead of introducing a second machine.
   Freshly extract elab and its copy counter and GUI witnesses. Calls, places,
   zones, copy policy and C/LLVM refinement remain OPEN in their assigned lanes.
   Claude's newer C1 call/borrow model changes the owner to elab(s,L,B).
   Forward B explicitly, adapt the harness (15 decision/24 refusal controls,
   30 fixed cases including borrowed-set width), and re-admit the changed
   core before extracting. Include caller loans, inout overlap refusals and
   the actual GUI function/procedure elaborations, not a handwritten call
   machine. A prior receipt never approves the changed source.
6. Cost: measure the extracted algorithm over fixed success/refusal workloads,
   recording exact inputs/toolchain/source hashes and semantic operation counts.
   Growth axes include statements, live-set width and loop nesting. Separate
   model and runtime cleanup costs. Never run ownership-negative programs or claim unimplemented
   cleanup runtime costs; do not optimize from source repetition alone.

Root is the sole implementation/integration owner; no parallel edit tracks.
Edit scopes: G0/G1 proof toolchain/install/CI wiring, corpus import prefixes,
reached proof gates, extracted elab tests, and scoped docs. The user's later
instruction authorizes only G items in the work-split directive. G2 has a
separate full-path revert inventory before execution. G3/G4/G5 wait for their
declared dependencies; this card does not grant permission to edit Claude's cores.
Forbidden overlap: native/self-host semantic implementation, installed compiler
pair, GUI/framework, registry CLOSED states, or old own/carrier/copy workarounds.

## Gates and budgets

Static owner/namespace/shell/CI checks: 60 seconds. Focused extracted model and
negative checks: 300 seconds. Fresh corpus kernel/self-test, certificate
adequacy and formal integration: 1800 seconds. Installation has its own bounded
process/log and is not a semantic pass. Performance uses reproducible fixed
input and repeats, not an expanded correctness timeout.

Shared integration gate: stable-toolchain admission -> fresh corpus kernel
check -> kernel negative self-test -> existing certificate extraction controls
-> cleanup extraction/negative controls -> bounded performance receipt.
Outputs remain implementation candidates until observed receipts exist.
No staging/commit/push, remote CI run or GUI readiness is authorized by this card.

## Observed G-scope receipt (2026-10-08)

- Stable Rocq core/runtime 9.3.0, Stdlib 9.2.0, and checksum-pinned opam 2.6.1
  are installed in the explicit WSL project prefix. System Coq is preserved.
- Fresh full-corpus and formal gates pass: 58 source files plus the approval
  export/binding consumer, with the existing two typed assumptions unchanged.
  All 58 recorded snapshot hashes match the current proof sources. The latest
  OwnershipCleanCore hash is
  d4cca76d5d3ff463126452ddfd928a936ca2e5be19bf91f640deee9c865b3c8b.
  The earlier line-1212 failure does not reproduce on this hash; the failing
  log is preserved rather than assigned an unproven cause.
- G1 uses only extracted elab/elab_fun/elab_proc. Its two-module kernel check
  has zero assumptions. Fifteen decision and 24 refusal controls pass,
  including the actual GUI call composition and borrowed/inout conflicts.
  Thirty fixed cost cases pass with source, artifact and input-recipe hashes
  in .tmp/ownership-cleanup/model-cost.json (schema v2). This is analyzer-model
  cost, not generated-program cleanup, D1 enforcement or compiler speedup.
- G2's 196 restored tracked files are base-equal; 234 new files were removed
  only after exact recovery copies were made. The 12 native/retention keep
  files match the pre-work backup. Source imports resolve (7536). The exact
  inventory and recovery boundary remain in the separate G2 directive.
- Version, checker, assumption-export, stale/orphan-artifact and direction
  negative controls pass. Focused proof consumers, certificate extraction,
  CI profile, language golden, beta readiness, hard-contract wiring and
  retention gates pass. Shell/YAML checks and diff hygiene pass.
- Whole world/component inventories did not finish in the existing 60-s
  local static budget; this is not a semantic verdict or full-inventory pass.
  Installed compiler/driver hashes are unchanged. Container execution,
  remote CI, DRV-2, backend/runtime ASan/LSan and GUI readiness are unverified.
- G3 has no admitted C2 interface yet (§5 is absent); G4 waits for C5 facts.
  G5 keeps baseline manual releases/analyzers until C3/C5/G3/G4 replacement
  gates exist. No parallel ownership model, old fallback or speculative ABI
  was introduced to bypass those dependencies.

Detailed navigation and gate logs:
docs/audits/ownership_clean_g_receipt_2026-10-08.md.

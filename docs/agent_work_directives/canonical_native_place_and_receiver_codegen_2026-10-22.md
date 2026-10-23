# Canonical native storage projection and receiver ABI (2026-10-22)

Status: IMPLEMENTATION CANDIDATE. Integrated base main `b1e2b532d`. The user
renewed canonical/installed-driver/CI closure, then transferred the remaining
temporary-backing semantic repair to the GPT root lane on 2026-10-22. GPT now
owns that final move-owner consumer, codegen, driver integration and final
verification. This is the already recorded native-codegen prerequisite, not a
new ownership precision track.

## Objective card

- Objective: emit the existing admitted storage/value contract correctly
  while compiling the canonical driver; do not rewrite canonical source.
- Priority: identity/current type and ABI facts, checked backing address,
  single evaluation, C/LLVM agreement, then patch size.
- Fact owners: existing MIR source-local expression types, nominal field
  layouts, lexical parameter storage and method ABI metadata; for the
  transient assignment loan, the function-parameter access/effect summary
  and builtin collection-effect identity consumed by the final default-
  aggregate move owner. The Array descriptor-preservation facet is not
  backing-address stability: `ArrayPush` preserves caller authority while it
  may still reallocate, so that facet cannot admit this loan by itself.
- Last consumers: native C assignment/member calls and LLVM field stores/
  calls, then native compilation of the installed self-host driver.
- Forbidden: semantic exemptions, copied descriptor writeback, unchecked
  array addresses, class/program-name routes, AST body reanalysis, new words,
  manual cleanup, or changing CI/time/memory admission to manufacture green.
- Gate/falsifiers: the two baseline semantic-admitted programs fail on both
  C/LLVM; repaired versions execute identically. Element bounds/negative
  indices still panic; target/index/RHS evaluate once in source order;
  readonly receiver writes and missing type/layout evidence still refuse.

## Reached chain and required changes

1. `pgy --native-pipeline` -> semantic admission -> HIR/DIR/RIR -> native MIR
   -> source-local expression/nominal layout and parameter ABI projections.
2. C MIR assignment calls the existing expression assignment owner. Its
   member target uses the array read owner, whose statement-expression
   currently returns a copied element, not a writable backing location.
   Preserve a checked element address in that same owner; value contexts
   still load/copy normally. Retain the existing getter's bounds/backing
   checks and evaluate receiver/index once. Do not reconstruct an owner.
3. LLVM member-lvalue projection currently handles identifier/member roots
   but not indexed roots. Share the existing checked-index IR owner between
   load and address consumers. Join the indexed result's MIR type with the
   nominal field layout; missing/mismatched facts fail closed.
4. C method-call receiver emission recognises an indirect-ref binding but
   still passes its pointer to a by-value self formal. Dereference only for
   the value ABI; retain pointer self semantics for identity-bearing hosts.
   LLVM has the same defect: load the host value from its existing lexical
   base-address owner, rather than loading the parameter's pointer type as
   the self argument. No source signature or parameter-summary change.
5. Preserve left-to-right target evaluation before RHS, and snapshot lexical
   entries before any scope mutation. Test nested member/index paths and
   ordinary read/value uses, not one program shape. The final native move
   owner currently walks `AST_ASSIGNMENT` as RHS then target indices; that is
   not the published target-before-RHS lane order. Its outer-to-inner loop
   also reverses nested `matrix[i][j]` indices; target evaluation must recurse
   through the receiver first, then evaluate that level's index. Correct both
   in the semantic change rather than copying either order into a backend.
6. Evaluating an indexed member receiver lends that backing until the
   assignment store completes. The final default-aggregate move/assignment
   analysis is the last consumer: it must use the admitted parameter-effect
   summary and builtin identity to refuse a later target-index operand or RHS
   that writes, grows, rebinds, releases or transfers the same backing. The
   descriptor-preservation answer cannot authorize a physical-address loan.
   The analysis must not add an AST body rescan or a second call-effect owner.
   Codegen must not repair this by reordering source evaluation or by silently
   recomputing a different place. The two falsifiers are
   `C:\Users\user\.codex\tmp\pergyra-canonical-codegen-20261022\indexed_rhs_growth.pgy`:
   a RHS call grows the backing after capture, and
   `C:\Users\user\.codex\tmp\pergyra-canonical-codegen-20261022\indexed_index_growth.pgy`:
   a target-index call grows it after the receiver descriptor is captured.
   Before the rule both are admitted and ASan reports heap-use-after-free on
   C and LLVM. The negative gate must refuse both before either backend
   publishes an artifact. The same-backing read-only control
   `indexed_same_backing_readonly.pgy` in the same frozen probe directory
   must remain admitted and print `7` on C/LLVM, as must unrelated RHS/index
   expressions and the existing indexed-member positive.

Baseline executable: `/root/gpt-mir-preservation-20261022/bin/pgy`, SHA256
`71f72fe6f99f1cbc3ccec676dba1eb203c907a595e763a96360c63f2fa0298fe`.
Frozen probes/logs: `C:\Users\user\.codex\tmp\pergyra-canonical-codegen-20261022\`
and that Linux root's `.tmp/canonical-codegen-baseline/`. `nested_member`
and `ref_receiver` both have zero semantic errors, then both backends fail.

## Integration order and limits

GPT edits only the reached final move owner, existing collection-effect query,
reached codegen owners, their native regression fixtures/gate and owned truth
state. Claude's other semantic/flow-corpus WIP is not copied.
Run the extended existing array-member gate, receiver/bounds/ordering
controls, unit suites and sanitizers first; then integrate completed Claude
commits and run unchanged canonical, actual bootstrap/installed driver,
gen2==gen3 and required exact-SHA CI. Budgets:60 s static,5 min focused,
30 min integration. A local native pass does not close the general
self-host emitter, any SoT row or the still-red canonical admission.

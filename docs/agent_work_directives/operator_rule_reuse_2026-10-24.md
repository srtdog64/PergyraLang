# Native operator rule reuse

Status: see [lane state](../truth/state/operator_rule_reuse.md).
Base: `ad920678bdaef3865f2d27f88e47a9cbb1d7d83e` (2026-10-24).
This is the user's bounded implementation request, not a new V1 rung.

## Objective card

- Objective: semantic checking and native C/LLVM share the exact overload
  vocabulary: supported tokens, suffixes and case-sensitive method aliases.
- Priority: existing language behavior, one rule owner, complete consumer
  migration, executable counterexamples, then patch size.
- Owner: `src/semantic/operator_overload_rules.c`; immutable vocabulary,
  independent of AST, MIR, program identity and backend representation.
- Last consumers: C role alias emission and binary calls; LLVM role forward
  declarations, bridge bodies and binary calls; semantic overload admission.
- Forbidden: private alias/suffix/token lists, compatibility wrappers, guessed
  aliases, source-text gates, or changing overload selection/ABI during reuse.
- Gates: vocabulary unit, exact-output native C/LLVM fixtures, rejected unknown
  aliases/wrong operand contracts, existing semantic and C emitter tests.

## Whole affected chain and scope

Source parser tokens reach `type_checker_expr_ops.c`. Free-function suffix
lookup precedes role lookup; role lookup walks local methods then includes.
The semantic pass checks arity and operand assignability. Preserve this order.

Native MIR currently carries declaration methods and expression tokens, not a
sealed per-expression overload target. C `transpiler_operator.c` reads that
inventory (legacy AST entrypoints also exist), then `transpiler_expr_core_emit.c`
and `transpiler_domain_role_methods_emit.c` emit calls/aliases. LLVM has equivalent
inventory readers in `llvm_domain_role_lookup.c`, forward/bridge consumers in
`llvm_domain_forward_role.c` and `llvm_domain_role_emit.c`, and a binary consumer
in `llvm_expr_scalar_core.c`. `llvm_expr_common.c` owns a fourth suffix copy.
`llvm_domain_method_helpers.c` owns the other LLVM copy and an alias table.
Their declarations live in `transpiler_operator.h`/`llvm_internal_emit_api.h`.

All these consumers will use the same immutable rule owner. Rule strings have
process lifetime; no allocation, transfer, release, lookup cache or mutable
state is added. C spelling, LLVM instructions, signature checks, include-depth
limits and callable symbol hygiene stay with their existing owners.

This is **vocabulary reuse, not operator-target SoT closure**. Native backends
still resolve targets from inventories. The audit's eventual semantic target
identity -> MIR -> emitters migration remains separate; self-host
`role_operator_vocabulary_owner.pgy` and its target carriage are not modified.

## Execution and integration

1. Fix baseline source and executable; add independent vocabulary and runtime
   expectations, including noncanonical aliases and rejection cases.
2. Move all native suffix/alias copies and supported-token enumerations to the
   owner; migrate declarations/callers and Makefile frontend sources together.
3. Run the narrow unit, baseline/candidate behavior comparison, negative cases
   and a wrong-rule mutation; broaden to semantic/C emitter suites.
4. Commit only this lane's paths on main. Main integration worker owns push
   and exact-published-SHA CI. Do not edit their dirty ownership/self-host files
   or global truth state; use a private temporary source export for builds.

Budgets: unit 60 s; focused parity 5 min; integration shard 30 min. No parallel
implementation lane, new Git worktree, canonical census or performance claim.
If a new consumer or pre-existing behavior divergence appears, map it here
before expanding the edit. Results are observations only after execution.

Baseline probe boundary: a free overload taking a `subject Box` by value and
called with temporary `Box() + 2` reaches a pre-existing native C ABI mismatch
(emitted Box value vs Box pointer parameter). This is not a vocabulary rule;
do not turn that failed probe into an accepted negative contract or broaden
this patch into subject argument lowering. Keep the runtime priority check on
the supported primitive Int route; record the subject falsifier separately.

Reached priority defect: the native LLVM role bridge body overwrites an
already-declared free operator's empty body before free-function emission.
Semantic checking and C choose the free function first. After the vocabulary
commit, make the LLVM bridge consult the existing MIR declaration inventory
(`llvm_decl_exists_in_context`, AST_FUNC_DECL) before materializing a role
bridge. No new AST rescan, selection cache or fallback is needed. The falsifier
expects 789 (free function), not 321 (role), in both source declaration orders.
Keep this behavioral fix and its runtime/negative gate in a separate commit.

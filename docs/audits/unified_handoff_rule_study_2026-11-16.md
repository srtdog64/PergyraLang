# A. One hand-off rule for both front ends

Status: proposal, read-only research. Nothing was built or run. Source was read at
commit `f1456722d` (branch `integrate/track-a`) with `git show`; the two November
decisions and the evidence file do not exist in the checked-out tree
(`fix/selfhost-landed-copies`). Verdict changes below are derived from reading the
two rule owners, not measured. The split of the 38 + 29 disagreements by cause is
unknown and is slice S0.

## 1. Survey

Each entry: what the source does, the one invariant worth importing, its premise,
and how it maps here. No architecture is imported.

| Source | Invariant worth importing | Premise | Mapping here |
|---|---|---|---|
| Racordon et al., "Implementation Strategies for Mutable Value Semantics", JOT 21(2), 2022. https://daveabrahams.com/wp-content/uploads/2022/02/implementation_strategies_for_mutable_value_semantics-preprint.pdf | A by-value argument is observably a copy. Sharing is an implementation choice that is legal only while nobody can tell. | Uniqueness is known at the mutation site (static, or a runtime count for copy-on-write). | Same source model as decisions 10-22 and 11-12. There is no runtime count, so "nobody can tell" has to be the static later-use column of the table. |
| Hylo language tour, "Functions and methods" (`let`/`inout`/`sink`/`set`). https://docs.hylo-lang.org/language-tour/functions-and-methods | The convention is a property of the callee's formal, and a `let` formal may not escape without a copy. The call site only checks liveness of the argument. | The convention is declared. | Here it is inferred, so the inferred class must be published as a formal fact that the call site reads. The call site must not re-derive it. |
| Swift SE-0377, "borrowing and consuming parameter ownership modifiers", Swift 5.9, 2023. https://github.com/apple/swift-evolution/blob/main/proposals/0377-parameter-ownership-modifiers.md | The convention changes who destroys the value, never the meaning of the program. A copy at a consuming call that is followed by a use is the compiler's job. | Copyable types with a whole copy. | This is decision 10-22. The premise is the "Clone copies whole" condition; where it fails the cell is a refusal. |
| Mojo manual, "Ownership" (`read`/`mut`/`var`, `^`). https://docs.modular.com/mojo/manual/values/ownership | Passing a still-used value to an owning formal copies; a last use transfers. One table of (convention, later use). | Declared convention, copy constructor exists. | Same table shape as section 2, with the convention column inferred. |
| Ullrich and de Moura, "Counting Immutable Beans", IFL 2019. https://arxiv.org/abs/1908.05647 | Owned or borrowed is inferred per formal as a least fixed point over the call graph: a formal is owned if the body resets, reuses, stores or passes it to an owned formal. The paper calls the inference a heuristic. | Whole-program bodies; a wrong choice costs only time because counts keep it safe. | The join and the transitive step are importable. The safety net is not: here a wrong class is a use after free, so a summary that is incomplete must join to "unproved", which moves. |
| Reinking et al., "Perceus: Garbage Free Reference Counting with Reuse", PLDI 2021. https://www.microsoft.com/en-us/research/publication/perceus-garbage-free-reference-counting-with-reuse-2/ ; Lorenzen et al., "FP²: Fully in-Place Functional Programming", ICFP 2023. https://www.microsoft.com/en-us/research/publication/fp2-fully-in-place-functional-programming-2/ | The rule is stated once as a syntax-directed calculus and the implementation is checked against it. `x = F(x)` updates in place exactly when the argument is unique at the call. | Runtime counts (Perceus) or a linear check (FP²). | The last-use move is the static version of the FP² premise. Import: "one written rule, checked", not the counts. |
| Lorenzen et al., "Oxidizing OCaml with Modal Memory Management", ICFP 2024. https://icfp24.sigplan.org/details/icfp-2024-papers/19/Oxidizing-OCaml-with-Modal-Memory-Management | Inferred modes (unique, local) form a small lattice with a join; an unannotated program still gets a principal answer. | Modes are part of the type and checked by one type checker. | Effect classes as a join-semilattice with a fixed, written join. |
| Lobster, "Memory management". https://aardappel.github.io/lobster/memory_management.html | Ownership of each formal is inferred from the body; where the proof fails the compiler inserts the fallback instead of asking the user. | The fallback (a runtime count) always exists. | The fallback here is `Clone`, and it exists only for whole copies. Otherwise refuse. |
| Vale, "Generational references". https://vale.dev/blog/generational-references | (Counterexample.) An unproved alias is made safe by a runtime check. | Every object carries a generation. | Not imported: the direction excludes runtime checks. It shows what the refusal cells cost in a language that chose otherwise. |
| Weiss et al., "Oxide: The Essence of Rust", arXiv 1903.00982. https://arxiv.org/abs/1903.00982 ; Rust project goal 2025H2 "Polonius" (compares the alpha analysis with a-mir-formality). https://rust-lang.github.io/goals/2025h2/polonius.html | A second implementation of an ownership rule is kept honest by differential testing against the first on a corpus, with every difference explained. | The corpus reaches the rule's cells. | The differential gate of section 3. The 868 fixtures reach only what both admit (278), so one fixture pair per table cell is added. |
| Villani et al., "Tree Borrows", PLDI 2025. https://pldi25.sigplan.org/details/pldi-2025-papers/42/Tree-Borrows | The alias model is one executable definition (a state machine per location) that every optimisation is checked against. | A dynamic checker runs the model. | A hand-off is a state transition of a binding row. Both front ends should emit the same transition rows, and a sanitizer leg is the dynamic check. |

What the survey says about the defect: every one of these systems has one place where
the callee's effect on a formal is named (declared, or inferred into a published
class), and one table from (class, argument liveness) to an action. None lets the
call site, or a later stage, recompute the class. Here two analyses compute the class,
two predicates turn it into "moves", and a third (the backend) guards against both.

## 2. The rule

### 2.1 Effect class of a default-mode aggregate formal

A set over six members, joined by union. It is computed over the formal's **storage
closure**: the formal, every local alias of it or of a member path of it that has no
landed `Clone` at its definition (decision 11-12: such an alias is a copy only when
the copy is made), and every record the callee builds from those.

| Member | Meaning | Native today | Self-host today |
|---|---|---|---|
| `Read` | descriptor, length or elements read | `READ` | effects 1, 3 |
| `Return` | the storage, or a part, or a record holding it, is the result | `RETURN_ESCAPE` | descriptor retention (return) |
| `Grow` | in-place change that can reallocate or rewrite the storage | `WRITE`, `array_storage_unproved` | effects 2, 6 |
| `Keep` | stored where it outlives the call: an `inout`/`ref` target, a channel, a moving formal of another callee | `CALL_ESCAPE`, `CHANNEL_ESCAPE` | descriptor retention (store) |
| `Release` | dropped | `RELEASE` | retirement |
| `Unproved` | summary missing, not final, or an opaque execution context | store failed / not complete | effects 4, 5 |

`moving = Grow | Keep | Release | Unproved`. A callee without a body keeps its declared
contract (unchanged). An `own` formal always consumes and is outside the table. A
struct constructor is a callee whose field formal has class `Keep`, and the record's
release recipe is the one releaser (decision 11-15).

### 2.2 Decision table

Later use = some path reads the binding's storage after the call and before its next
definition. `x = F(x)` is not a later use. A site that runs again without a new
definition of the binding (a loop that does not declare it, a `defer`, lambda, `async`
or `parallel` body) counts as "used again".

| Class | Argument origin | Later use / repeat | Action |
|---|---|---|---|
| `Read` only | any | any | **Lend for the call.** Row unchanged; the caller still releases. |
| `Return`, no moving member | fresh value | – | Result owns it. No row. |
| same | named local | none, runs once | **Hand off.** The result owns what it took. |
| same | caller's own formal | used again | **Loan to the result** (decision 10-19), if the loan proof holds. Otherwise as the next row. |
| same | named local, or a refused loan | used again | **Clone at the argument** if `Clone` copies whole; else **refuse**, naming callee and formal. |
| any moving member | fresh value | – | Moved. No row. |
| same | named local, one definition | none, runs once | **Hand off.** Neither side releases (11-15); a record releases a constructor input once. |
| same | named local | used again, or repeats | **Clone at the argument** if whole (10-22); else **refuse**. |
| same | caller's default-mode formal | none | Moved on; the class joins into the caller's own formal (the transitive step). No row. |
| same | caller's default-mode formal | used again | Clone if whole; else refuse. |
| same | borrowed: `ref` formal, live loan, element borrow | any | The caller does not own it: Clone if whole; else refuse. |
| `Unproved` in a detached, `async` or `parallel` context | any tracked container | any | **Refuse** (existing worker-boundary rule; no hand-off). |

Not in the table, unchanged: member-path arguments (`F(r.items)`) stay with the
member-move owner and decision 11-12; a binding with several definitions gets the
action per call edge, but its single row still records a hand-off only for one
definition handed once.

### 2.3 Consistency with the decisions

- 10-18: every moving member moves; the fresh-value and `x = F(x)` rows are its two allowed callers.
- 10-19: the loan row, limited to the caller's formal as decided.
- 10-22 and 10-25: Clone only where it copies whole; no source `Clone` is added by this rule.
- 11-12: the alias closure stops at a landed `Clone`. Without one, `var grown = values; push(grown)` is `Grow` of `values`.
- 11-15: hand-off releases nothing; the table adds no callee release.

### 2.4 Where a current verdict would change

1. **Self-host, alias growth.** `GrowCount(names)` (`grown_handoff`) becomes a hand-off. Today the row stays live and the backend refuses the program. The gate `direct_mir_array_string_cleanup_fact_owner.sh` would change `refuse grown_handoff` to an admitted, clean run.
2. **Native, descriptor inside a returned record.** `RowsFill(seed, 4)` in `exclusive_branch_rebind_positive`: an `Array` formal stored in a record that is grown and returned is `Grow | Return`, so `seed` is handed off instead of staying live. Native cleanup follows its privacy proof, so its emitted C and LLVM should not change; that is an acceptance check, not an assumption.
3. **Self-host, returned constructor inputs.** `retired` becomes `handed-off`. The backend releases nothing in either state.
4. **Self-host, `Unproved`.** Effects 4 and 5 leave the row live today and the backend refuses. Synchronous `Unproved` becomes a move, as native does; detached contexts are refused at the semantic owner.
5. **Clone against hand-off.** Under one later-use definition only one answer survives per site. Which front end changes at each site is unknown until S0.
6. **Repeat sites.** The refusal moves from the backend (`grown_loop`) to the semantic owner, or becomes a landed Clone where the copy is whole.

## 3. One definition, two consumers

### 3.1 Objective card

- Objective: both front ends issue the same hand-off action at every call edge of every program both admit, from one written rule.
- Fact owner: `src/semantic/call_handoff_rule.def`, listed under the existing row `semantic.hashmap_collection_ownership`. No new SoT row.
- Last consumer: the MIR collection ownership transition (`mir_json_dump.c` → `direct_mir_scalar_program_collection_ownership_transition_admission_owner.pgy` → `ArrayStringLocalCleanupRequired`).
- Forbidden: a backend recomputation of the class, a per-fixture exemption list in the gate, a weakened refusal, a new annotation.
- Gate: the differential gate G2 at 0 native-only and 0 self-host-only.

### 3.2 Shape

The `.def` holds three declarative parts: the class members with their join, the origin
and later-use vocabularies, and the action table of 2.2. `scripts/render_call_handoff_rule.py`
renders it into a C header and a `.pgy` owner, with `--check` for drift. This is the
pattern `builtin_argument_retention_registry.def` already uses, and generator drift is
an allowed static check.

The table alone does not close the defect. Most of the disagreement is in how each
front end computes the class (alias closure, record return), not in the final
predicate. So the plan binds the two analyses at two published points:

1. each front end maps its own summary bits to the generated class set through a generated projection (the two right-hand columns of 2.1), and
2. the class of every default-mode aggregate formal is written as a row of the routine in MIR JSON, beside the binding rows that already carry `handed-off`.

The two analyses stay two programs until the native front end is retired. Agreement is
then held by a gate, not by construction; that is the honest limit of this option. The
alternative "self-host owns it and native reads it" is not available: native compiles
the self-host first, so it cannot depend on self-host output.

### 3.3 Old paths to delete

- The hand-written logic in `semantic_default_aggregate_formal_moves`, `..._formal_lends` and `..._formal_borrow_handoff` (`default_aggregate_move_rule.c`); they become lookups in the generated table.
- The `moving` expression in `ast_collection_call_handoff_owner.pgy`, the numeric effect codes 1–6 and `SemanticAstCollectionFormalEffectFromFlags` (`ast_collection_formal_effect_graph_owner.pgy`), replaced by the generated class set and join.
- In `direct_mir_scalar_program_array_string_storage_sharing_owner.pgy`: the by-value call branch (result code 2) and the String-element mode of the descriptor preservation proof. A MIR validation takes its place: a live row passed whole to a formal whose class row has a moving member, with neither a hand-off nor a Clone, is invalid MIR.

### 3.4 Gates (all compile or run something)

- G1 drift: `render_call_handoff_rule.py --check`.
- G2 differential: over the 868 lifetime fixtures and `tests/cases/function_param_flow_summary`, for programs both admit, the sets of (routine, formal, class) and (routine, binding, call edge, action) from the two MIR JSON outputs are equal. Baseline 119 / 38 / 29. It is a measurement that writes evidence until S5, then a required gate at 0 / 0.
- G3 negative: MIR with the hand-off removed, with a hand-off forged on a `Read` formal, or forged on a member move, is refused by the MIR validator on both backends and on the `--mir-json` route.
- G4 runtime: every program admitted in G2 runs on C and LLVM with equal output, under ASan and at `-O0` under glibc heap checks (the 11-16 evidence shows `-O1` ASan missed a double free).
- G5 cells: one positive and one negative fixture per table row, compiled by both front ends.
- G6 integration: the same class and action comparison on the DRV-2 input, within the 30-minute shard.

### 3.5 Migration order

| Slice | Change | Acceptance |
|---|---|---|
| S0 | Scratch instrumentation only: attribute each of the 67 disagreements to a table cell. | Evidence file; every disagreement has a cell or is listed as unknown. No code lands. |
| S1 | `.def`, renderer, drift check. Both front ends call the generated action with their existing classification. | Same verdicts and same C and LLVM bytes on the 868 and `tests/cases`; gen2 == gen3. |
| S2 | Class rows in MIR JSON from both; G2 as a measurement. | Reproduces 119 / 38 / 29 at the action level. |
| S3 | Self-host alias closure. | `grown_handoff` agrees and runs clean on both legs; no new sanitizer report. |
| S4 | Native: `Keep` and `Return` inside a record for an `Array` formal. | The seed of `exclusive_branch_rebind_positive` agrees; native bytes unchanged, or stop and report. |
| S5 | One later-use and repeat definition; `retired` → `handed-off`; `Unproved` moves. | G2 at 0 / 0 and required; G4 and G5 pass. |
| S6 | Delete the backend branch; add the MIR validation. | G3 passes through the validator; the `--mir-json` route admits at least 90. |
| S7 | Delete the old predicates and effect codes; record the rule as one decision file. | G1–G6; the registry text for the row drops the "two rules differ" remainder. The row is not claimed closed by this. |

## 4. Risks and questions for the user

1. **A getter on a local that is used again.** (a) Keep 10-19: it moves, so a Clone lands or the use is refused. (b) Extend the loan to locals with "no transfer while a loan lives". Recommend (a) for V1 and count the landed Clones in S0.
2. **`Unproved` class.** (a) Move, as native does: admitted, storage leaks. (b) Refuse. (c) Move when synchronous, refuse in detached contexts. Recommend (c).
3. **Class row in MIR.** Decision 11-15 did not choose a consuming-parameter fact in the MIR signature (for callee release). This row is narrower: it is read only by validation. (a) Add it and delete the backend guard. (b) Delete the guard with no row and rely on G2. (c) Keep the guard. Recommend (a); (b) loses the forged-MIR negative.
4. **Form of the single source.** (a) `.def` rendered to C and `.pgy`. (b) Two hand-written rules plus G2 only. Recommend (a); when native retires, the `.pgy` output is the only one left.
5. **Leak hides errors.** Because a hand-off releases nothing, "hand off" against "Clone" cannot be told apart at run time, so G2 is the only detector. Keep 11-15 as is; the class row is what a later callee-release decision would read.
6. **Native snapshot order.** `f1456722d` records that marking hand-offs before the effect snapshot changed a verdict. S4 changes which rows native marks, so it can disturb that replay. S4's acceptance is byte equality for that reason.
7. **Coverage.** Only 278 fixtures are admitted by both, and self-host LLVM admits 167 of 868. Agreement on them says little about the canonical program; G5 and G6 exist to cover that, and G6 costs native analysis time that is already the first blocker.

## 5. A getter on a local that is used again (follow-up to 4.1)

User decisions recorded since section 4: `Unproved` moves in a synchronous call and is
refused in a detached context (4.2 c); the MIR class row is added and the backend guard
deleted (4.3 a); one `.def` is rendered to C and `.pgy` (4.4 a). Question 4.1 was sent
back with "look at other languages and find the better way". This section answers it.
Nothing here was built or run.

### 5.1 How other languages let a caller keep using the whole

| Source | Invariant | Premise | Developer cost |
|---|---|---|---|
| Rust, RFC 2094 "Non-lexical lifetimes" (2017; default since 1.63, https://blog.rust-lang.org/2022/08/05/nll-by-default/) | A loan is live from the call to the last use of the result and of what is derived from it. Only a conflicting access of the owner inside that span is an error. | The signature says which argument the result borrows from. | `&`/`&mut` at every site; lifetime names when elision cannot tell which argument is returned. |
| Rust, RFC 2025 "Nested method calls" (two-phase borrows) | A borrow taken for a later mutation is only a read until the mutation starts, so `v.push(v.len())` is legal. | The reservation and the activation are distinct points in MIR. | None beyond Rust's own. |
| Rust, "Enabling the next iteration of the borrow checker on nightly" (Polonius alpha), Rust blog, 2026. https://blog.rust-lang.org/2026/08/04/enabling-polonius-alpha-on-nightly | Liveness is judged per path: a loan that leaves on one branch does not block the owner on the other. | A location-sensitive analysis that was too slow for years. | None for the user; the cost is in the compiler. |
| Swift, SE-0176 "Enforce Exclusive Access to Memory" (Swift 4/5) | Two accesses of one variable may overlap only if both are reads. Most accesses are instantaneous; `inout` and accessors have a duration. | Each access has a known begin and end; the residue is checked at run time. | No annotation; a runtime trap for what the static check misses. |
| Swift, SE-0474 "Yielding Accessors" (accepted 2025, https://forums.swift.org/t/accepted-se-0474-yielding-accessors/80273) and SE-0507 "Borrow and Mutate Accessors" (accepted 2026, https://forums.swift.org/t/accepted-se-0507-borrow-and-mutate-accessors/85266) | An accessor lends a part without a copy, and the part cannot outlive the caller's access of the base. | The accessor is declared as a borrow, not a `get`. | A keyword on the accessor; nothing at the call. |
| Hylo, "Subscripts" (projections, `yield`). https://docs.hylo-lang.org/language-tour/subscripts | A projection is second class: it cannot be stored or returned, and the whole is not mutable while it lives. No lifetime exists to name. | The callee is written as a subscript. | A different declaration form; no lifetimes. |
| Mojo, "Lifetimes, origins, and references". https://docs.modular.com/mojo/manual/values/lifetimes | The origin of a `ref` result is inferred from the argument, and the argument stays alive as long as the result is used. | `ref` results are declared; origins are parameters. | `ref` and, for non-trivial cases, explicit origins. |
| C# 11, "Low Level Struct Improvements" (ref safety, `scoped`). https://learn.microsoft.com/dotnet/csharp/language-reference/proposals/csharp-11.0/low-level-struct-improvements | With no lifetime names at all, the result of a call may escape no further than the narrowest scope of the arguments it could refer to. | A fixed, conservative rule per parameter kind. | `scoped` only to opt out of the conservative answer. |
| Vale, "Zero-cost borrowing: regions overview". https://vale.dev/blog/zero-cost-borrowing-regions-overview | Inside a region marked immutable, everything read from it is borrowed without a check. | The scope is marked `pure`; otherwise a generation check runs. | An opt-in marker, or a runtime check. |
| Lorenzen et al., "Oxidizing OCaml with Modal Memory Management", ICFP 2024 (cited in section 1) | A `local` value may be read anywhere in its region and may not leave it; the mode is inferred. | Regions are lexical. | None when inferred. |

The RFC 2094, RFC 2025 and SE-0176 texts were not fetched in this session. Their content
is stated from memory, and the dates should be checked before they are cited elsewhere.

What they agree on. No language that avoids both the copy and the runtime check gives up
the loan. They differ only in how the two facts the loan needs are obtained:

- which argument the result borrows from: declared in Rust, Mojo and Swift, implied by
  the declaration form in Hylo, assumed conservatively in C#;
- how long the loan lives: the last use in Rust, the caller's access in Swift, the
  lexical region in Hylo and OCaml.

This language already has both facts without an annotation. The class row of 2.1 says
`Return` for exactly the formals the result can share storage with, which is more
precise than C#'s assumption. The later-use predicate of 2.2 is the last-use liveness of
RFC 2094. Decision 10-19 limited the loan to the caller's own formal because a local
can be transferred while it is lent, and it named the missing rule itself: a transfer is
refused while a loan of the same owner lives.

### 5.2 The rule

A `Return`-only call lends **any** owner that the caller uses again, a local as well as
a formal. The result is a loan of that owner.

- **Second class.** The loan may be read, passed to a `Read` formal, or passed to a
  `Return`-only formal, which makes a derived loan of the same owner. It may not be
  changed, handed to a moving formal or a constructor, stored where it outlives the
  owner, captured by a detached body, or returned. Returning stays allowed for an owner
  that is the routine's own formal whose summary also returns it, as 10-19 decided.
- **Live to the last use.** The loan lives while the result binding, or a binding
  derived from it, has a later use. This is the same predicate as the "later use"
  column of 2.2, so the `.def` defines it once.
- **Conflict.** While a loan of an owner lives, a moving use of that owner or of its
  storage closure is a conflict: in-place growth, a hand-off, a constructor input, an
  `inout` argument, a reassignment, a drop.
- **Fail closed.** A conflict or an exit takes the copy ladder that the decisions of
  10-20 and 10-24 already fixed: copy where the loan leaves, else `Clone(call)`, else
  `Clone(argument)`, each only where `Clone` copies whole. Otherwise the use is refused,
  naming the callee, the formal and the conflicting use. Nothing is admitted without
  either a proof or a whole copy.
- **Release.** The owner's row stays live and the owner releases as before. The result's
  row has origin `lent` and releases nothing.
- **The hand-off cell is unchanged.** A `Return`-only call on a local with no later use
  of the local and no live loan of it still hands off: `return RowsError(code, ids)`.

Flow sensitivity is deliberately the plain kind: the loan is live at a point when some
path from it reaches a use. The per-path refinement of Polonius is not proposed; AGENTS
asks for precision beyond the canonical program to be found in use.

### 5.3 Decision table (replaces the "used again" `Return` rows of 2.2)

| Event while a loan of owner `O` through result `R` is live | Action |
|---|---|
| read of `O`; `O` to a `Read` formal | admit, no copy |
| another `Return`-only call on `O` | admit, a second loan |
| read of `R`; `R` to a `Read` formal; `R` to a `Return`-only formal | admit, no copy (the last makes a derived loan of `O`) |
| `R` changed in place, or bound by a `var` that is changed | copy where `R` is taken (11-12), if whole; else refuse |
| `R` to a moving formal or a constructor | `Clone` at that argument (10-22), if whole; else refuse |
| `R` returned, stored in a place declared outside the scope of `O`, or sent on a channel | copy at the exit, if whole; else refuse. Admitted only for `O` a formal that the routine's own summary returns |
| `R` captured by `defer`, a lambda, `async` or `parallel` | refuse |
| `O` grown, handed off, given to a constructor, passed `inout`, reassigned or dropped | `Clone(call)` where the loan was made, else `Clone(argument)`, if whole; else refuse |
| the lending call repeats in a loop and `R` is declared outside that loop | the loan is live for the whole loop body; the rows above apply |
| the callee's class row is `Unproved` or has a moving member | not a loan; table 2.2 applies |

Consistency:

- 10-18: a moving callee still moves.
- 10-19: the loan and its proof are kept. Only "a formal of the calling routine" is
  widened to any owner, under the transfer rule that decision named as its condition.
- 10-22: a moving use still takes its copy at the argument.
- 11-12: a `var` taken from a loan is a copy as soon as it is changed, and no write-back
  is added.
- 11-15: no release is added on either side.

Where it changes a verdict: a getter on a local that is read again no longer lands
`Clone(argument)` (native) and is no longer refused or copied by the descriptor-retention
owner (self-host). Every other row is the outcome the decisions already give.

### 5.4 Programs

Must stay refused, or land a whole copy where one exists. Without that, each is a use
after free or a wrong result.

| | Program shape | Why |
|---|---|---|
| F1 | `let ids = Ids(rows); ArrayPush(rows.ids, 9); Log(ids[0])` | growth can reallocate what `ids` reads |
| F2 | `let a = First(arena); let t = Consume(arena); Log(a.name)` | transfer under a live loan (the case 10-19 named) |
| F3 | `let ids = Ids(rows); let out = Wrap(ids); Log(rows.ok); return out` | the loan leaves a routine whose local owner ends |
| F4 | `var keep = []; { let rows = Make(); keep = Ids(rows); } Log(keep[0])` | stored outside the owner's scope |
| F5 | `var last = Ids(seed); while ... { Log(last[0]); ArrayPush(rows.ids, i); last = Ids(rows); }` | live across the back edge |
| F6 | `let r = Pick(x, y, false); ArrayPush(y.items, 1); Log(r.items[0])` | the per-owner falsifier of 10-24 |
| F7 | `var ids = Ids(rows); ArrayPush(ids, 1); Log(ArrayLength(rows.ids))` | change through the loan; 11-12 says copy |
| F8 | `let ids = Ids(rows); defer { Log(ids[0]); } rows = Other();` | captured loan and a reassigned owner |
| F9 | `Grow(Ids(rows)); Log(rows.ok)` | a loan handed to a moving formal |

With `Ids` returning an `Array<Int>` member these land one copy. With an Array of
structs, where `Clone` is shallow, they are refused. Both variants belong in the gate.

Must run with no `Clone` in MIR, equal output on C and LLVM, and clean under the
sanitizer:

| | Program shape |
|---|---|
| P1 | `let n = Name(cfg); Log(n); Log(cfg.ok)` with `cfg` a local |
| P2 | `let a = AtomText(arena, 0); let b = AtomText(arena, 1); Log(a); Log(b); Log(arena.ok)` |
| P3 | a getter on a local table inside a loop, the table read after the loop |
| P4 | `let n = ArrayLength(Ids(rows)); ArrayPush(rows.ids, n)` (the loan is dead before the growth) |
| P5 | `let b = Block(arena, 0); let ids = Ids(b); Log(ids[0]); Log(arena.ok)` (derived loan) |
| P6 | `let a = First(arena); Log(a.name); let out = Consume(arena)` (hand-off after the loan died) |
| P7 | `Log(Name(Block(arena, 0)))` (temporaries within one statement) |
| P8 | `return RowsError(code, ids)` (the hand-off cell, unchanged) |

### 5.5 What the `.def`, the class row and the slices gain

- `.def`: one action `LoanResult`, and the table of 5.3 as a third declarative part
  (use kind and loan state give an action). The later-use predicate is already there.
- MIR: one loan row per lending call edge, with the routine, the call edge, the owner
  binding, the result binding or temporary, and the outcome (`stays`, `copied-at` a
  place, `refused`). The result's binding row carries origin `lent`.
- MIR validation, which replaces the backend guard in S6: a `lent` row whose owner row
  is handed off, retired or redefined at a point where the loan still has a later use is
  invalid MIR, and a `lent` row is never released.
- Gates: G2 also compares loan rows. G5 gains F1–F9 in both copy variants and P1–P8,
  with "no `Clone` in MIR" checked on the emitted MIR of the positives.

| Slice | Change against 3.5 |
|---|---|
| S0 | Also count, on the fixtures and on the DRV-2 census, the getter-on-local sites and the copies they land today. That is the size of the gain, and it decides whether S5b belongs in V1. |
| S5 | Unchanged, but it now also fixes the later-use predicate that S5b depends on. |
| S5b (new, after S5) | Native: the lent proof accepts a local owner, keyed per owner as 10-24 requires, with the transfer-under-loan conflict. Self-host: the same table from the generated owner. Acceptance: P1–P8 emit no `Clone` and run clean on both legs; F1–F9 are refused or copied identically by both; G2 loan rows are equal; canonical census refusals do not rise; native analysis time on the canonical input is measured before and after. |
| S6 | The MIR validation includes the `lent` rules above. |
| S7 | A decision file that widens 10-19 from "formal" to "any owner". 10-19 is a lane decision the user can overturn, so this needs the user's sign-off. |

### 5.6 Risks

1. **The self-host has no general loan proof.** It has the MapGet loan and the indexed
   String borrow owners; native has `function_param_flow_lent_argument_admitted`. S5b
   builds a second implementation of a rule that depends on liveness, which is where the
   two front ends already diverge. The table is kept second class and not path sensitive
   so that both can implement it the same way. G2 on loan rows is the only proof that
   they do.
2. **Analysis time.** Native analysis time is blocker 1, and the lent proof walks a loan
   again up to 16 times. More loans mean more walks.
3. **Native keeps leaking.** Native releases a local only under its privacy proof, and a
   loan of a local may break that proof. Native would then admit with no copy and release
   nothing, while the self-host releases the owner. Output stays equal, and the
   difference is invisible at run time, as in risk 4.5.
4. **Fewer copies can expose hidden defects.** The `Clone(argument)` landed at these
   sites today hides any error in the storage closure of 2.1. Without it, the alias
   closure and the class row carry the safety of these sites, so S5b must not land
   before S3–S5 reach 0 / 0.

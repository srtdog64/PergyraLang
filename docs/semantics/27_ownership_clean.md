# Ownership Clean: Compiler-Owned Cleanup Model

Status: `formal-model; implementation OPEN`.
Proof: [`proofs/OwnershipCleanCore.v`](proofs/OwnershipCleanCore.v), with
[`proofs/OwnershipCleanComposition.v`](proofs/OwnershipCleanComposition.v),
[`proofs/OwnershipCleanReadOnly.v`](proofs/OwnershipCleanReadOnly.v), and
[`proofs/OwnershipCleanExits.v`](proofs/OwnershipCleanExits.v)
(no axioms, no admits; checked by Rocq 9.3.0 and `rocqchk`).
Direction: `AGENTS.md` (2026-10-08) selects compiler-owned ownership cleanup,
not a tracing GC. It also rules out manual deep drop, field carrier/restoration
choreography, and extra `own`/`ref` annotations as the default answer to
missing compiler inference.
Related design review: [`../audits/ownership_dx_architecture_recheck_2026-10-08.md`](../audits/ownership_dx_architecture_recheck_2026-10-08.md).

This document fixes the algorithm that turns a value-semantics program into
one that releases every heap block exactly once, at its last use, with no
source annotation. The Rocq files prove cleanup and simulation for admitted,
terminating core programs, assuming source execution and admission of called
bodies. They do not prove that inference admits every source program or that
every production source construct lowers to this core. Nothing here claims
that the native or self-hosted compiler implements it yet.

**Mechanism.** The adopted name is in section 0. The mechanism behind it is
static ownership elaboration: the compiler decides, at compile time, where
each value is moved, copied, and released, and inserts those operations into
the program. There is no tracing, no reference count, and no collector at
run time. Ordinary values retire at their certified last use; declared region
lifetimes and observable resource cleanup edges remain their own boundaries.
This is not a claim that every allocator returns pages at the last source
read. "Automatic"
means that the source contains no release code, not that a runtime finds
garbage.

## 0. Adopted name and comparison boundary

Adopted on 2026-10-08 at the user's direction: **ownership-based automatic
memory management** (`소유권 기반 자동 메모리 관리`). The authoring goal is
**GC-like convenience from ownership evidence**. This is Pergyra's core
ordinary-value lifecycle mechanism, not an optional collector mode. The
existing source semantics, one-owner machine, inferred conventions, D1 copy
policy and C2 implementation interface below remain unchanged.

The mechanism is automatic; it is not a tracing GC. Logical retirement and
physical allocator reuse are different obligations. Batching or region reuse
may consume verified lifetime facts, but must not create a second owner,
delay observable resource finalization, or hide an unproved lifetime behind
a GC/RC fallback. Files, sockets and authority-bearing handles retain their
declared cleanup edges.

The importing supplement
[`proofs/OwnershipCleanGCComparison.v`](proofs/OwnershipCleanGCComparison.v)
bounds comparisons. Under canonical `INV`, the exact owner/frame heap has
no more blocks than a duplicate-free collector heap covering those same live
blocks. Read coverage permits deferred garbage; ownership additionally
requires exact reclamation. An exact read-covering heap exists:
`correct_gc_can_match_ownership` chooses the existing `INV` heap as
a read-covering collector heap. It is not a collector implementation or a
GC memory-safety theorem. Read coverage is weaker than complete memory safety;
these results do not establish that GC is less memory-safe.

For a common allocate/observe/retire workload, canonical `elab` and `texec`
remain the executable semantic authority. Compared with an ideal-root,
nonmoving full-heap sweep under equal allocation/copy/release costs, ownership
avoids that sweep's inspections. A strict abstract cost advantage requires
positive inspection cost and nonempty work. The sweep's visit count follows
its recursion; the allocation/release cost formulas are separately specified,
not costs instrumented from `texec`. Zero inspection weight gives equality in
those formulas. Different allocator/copy/barrier profiles give arithmetic
witnesses in both directions, not executions of those alternative policies.
In particular, `bulk_reset_cost_refutes_universal_speed` is the inequality
`1 < 3`; no bulk-reset allocator or immediate-collection cost algorithm is
defined. Those are unmodelled comparison cases, not proved performance
counterexamples. None establishes wall time, general or generational GC speed,
or universal strict superiority.
The proof/audit gate is `tests/ownership_gc_comparison_smoke.sh`; compiler
refinement, actual C/LLVM performance and installed-driver evidence stay OPEN.

Identity-bearing graphs add separate retention, nullable-link, indexing,
loan and finalizer costs; see
[composition tradeoffs and limits](28_memory_boundary_composition.md#tradeoffs-and-limits).
The [Qt provenance](28_memory_boundary_composition.md#design-provenance)
credits the user's graph/teardown proposal, not the ordinary-value
liveness and move/copy algorithm specified here.

## 1. Why: the measured starting point

The figures below describe the earlier 2026-10-08 source snapshot, before the
later rollback/deletion inventory. They are historical starting-point
observations, not today's counts. P0 must rebind counts/locations to its exact
source manifest; do not mix them with the cutover plan's later inventory.

- **No general automatic last-use release.** Existing generated explicit
  release and bounded cleanup paths are not this ownership-clean pass.
  Copying a value generally copies the descriptor; the backing is
  shared. `llvm_expr_aggregate.c` calls this "correct under Pergyra's no-free
  model". The scope-exit releases that do exist cover Slot, zone/world/effect
  handles, `defer`, and string-concat region temporaries.
- **The storage model has to change first.** Under today's shared backing,
  inserting a drop at each last use frees one backing once per descriptor
  copy, which is a double free (`alias_copy_double_free`). Drop insertion
  alone is therefore not a partial step toward this model; it is a bug.
  Landing the model replaces the storage model first: one owner per backing,
  and deep copy glue wherever a fact says copy (section 5.4). Only then are
  drops emitted.
- **Manual release in the compiler.** The self-hosted compiler calls
  `CompilerRetireArrayStorage` 212 times, `ArrayDrop` 148 times, and
  `ArrayDropOwnedStrings` 87 times. The release-proof analyzer that admits
  those calls is 34 `ast_collection_aggregate_*` files, 5,808 lines. Its
  refusals of the compiler's own releases are the current DRV-2 blockers. The
  retirements exist to fit the 3 GiB build-pressure cap (recorded peak 2.974
  GiB), not for correctness.
- **Authoring cost.** The source has to detach a field, call, and restore it,
  because `inout` refuses a member path such as `Push(h.items)`. Results have
  to be threaded through `own` and rebound. Temporaries have to be bound to
  named locals before readonly boundaries. In compiler source, the
  parameter-mode annotations went from 71 (2026-07-01) to 6,849 (2026-10-08,
  dirty tree).

The ownership-clean model moves that work into the compiler.

## 2. The model

### 2.1 Source language: value semantics only

The source has no move, drop, `own`, `ref`, or clone. Every read observes an
independent value, and nothing is ever freed. The core statements are:
computed definition, copy `x := y`, aggregate construction `x := Node[ys]`,
push `x := x ++ [y]`, field/element read `x := y.i`, observation, sequencing,
`if`, `while`, a function call `x := g(ys)`, an inout call
`g(inout z, ys)`, a focus on a part (step 8), an unpack of a record into its
parts (step 9), and a region value (step 10). The exits layer adds `break`,
`continue`, `return`, `throw`, and `try` (step 11). Parameters have no mode in
the source; a parameter is just a value. This semantics is the reference: elaboration must not change
what it observes.

### 2.2 Target: the ownership machine

Each runtime value owns a list of heap blocks, exactly one block per node of
the value, in preorder. One allocation step creates all the blocks of the
value it builds, so a deep copy allocates one block per node, and the blocks
of any part can be found from its path. A frame has owned bindings
and borrowed bindings. A borrowed binding can be read but never moved,
mutated, or freed; its blocks belong to an enclosing frame (the frame heap).
A call moves its sink arguments into the callee frame, where they are owned,
and lends its borrowed arguments.
The machine keeps an explicit live-block heap. Reads and explicit drops have
live-block guards; consuming operations require owned bindings, fresh
destinations and fresh allocation blocks where their rules specify them.
Those guards refuse invalid reads/drops, consumption through a borrow,
owner overwrite and live-block reuse.

This is not a claim that every malformed raw target state gets stuck:
`TE_Unpack` removes the record's node with `free [b0] H` without a separate
live-node premise. For admitted programs, `INV` supplies the live, unique
record footprint, and its preservation prevents invalid or repeated frees.
The safety argument therefore uses the invariant as well as rule guards.

### 2.3 The elaboration algorithm

The input is the set `L` of variables live after the statement, computed by a
backward liveness pass, and the set `B` of borrowed variables (the enclosing
function's readonly parameters).

1. **Move or copy.** A use of `y` that is still live afterwards (`y ∈ L`) or
   borrowed (`y ∈ B`) is a copy, which allocates fresh storage. Otherwise it
   is a move: the storage is transferred and `y` is unbound.
2. **Consuming constructors.** Aggregate construction and push consume their
   element operands. If an element is still live, the compiler first copies it
   into a temporary and moves the temporary in. A dead element is moved
   directly.
3. **ASAP release (`settle`).** After each statement, every variable that was
   live before and is dead after is released. An owned variable is dropped,
   which releases its whole footprint, deep by construction. A borrowed one
   only ends, with no heap effect.
4. **Branches.** Each arm starts by dropping the variables live into the `if`
   that the arm no longer needs. Both arms end with the same live set, so the
   join needs no runtime drop flag.
5. **Loops.** A loop carries a head live set as a certificate. Elaboration only
   checks it: the body's live-in, the exit set, and the condition must all be
   included in it. The liveness solver is therefore outside the trusted base.
   On entry the body drops what is redefined before use; on exit the loop drops
   what is dead afterwards.
6. **Calls.** Each parameter has a mode, borrowed or sink. The mode table is an
   input to elaboration, like the loop head.
   - A borrowed argument is lent. The callee body is elaborated with its
     borrowed parameters in `B` and only its result live at exit, so it can
     read them and copy out of them but cannot release caller storage.
   - A sink argument is consumed. The caller moves it into the callee frame
     at its last use. If the caller still needs it (it is read later, also
     lent to the same call, or itself borrowed by the caller), the caller
     first copies it into a temporary and moves the temporary.
   - Parameters the body never uses are released on entry: sink ones are
     dropped, borrowed ones end.
   - The result's ownership returns to the caller.
   - In this core, an inout call has one inout argument: its first sink
     argument is also its result. The argument moves in and the updated value
     moves back, with no copy. A production call can have several inout
     arguments and an independent result. Its ordered normalization and
     recovery on every exit are separate refinement obligations (§5.10), not
     an already proved consequence of `SCallIO`.
   - The core refuses a call that passes one variable to two sink
     parameters. The implementation binds a temporary first.
   - **Summaries fail closed.** The table holds, for each routine, either a
     resolved summary (one `borrow` or `sink` entry per parameter) or none.
     A call through a routine with no summary is refused. So is a summary
     whose length differs from the call's argument count, and a routine
     whose summary length differs from its parameter count. A missing entry
     is never read as `borrow`: an unresolved summary and a verified
     all-borrowed one are different values (`missing_summary_refuses_call`,
     `arity_mismatch_refuses_call`, `missing_summary_refuses_routine`,
     `arity_mismatch_refuses_routine`).
7. **Mode inference.** A parameter is sink when the callee body stores it,
   moves it, mutates or redefines it, returns it, or passes it on to a sink
   parameter. Every other parameter is borrowed. Elaboration is sound for
   every mode table under the function/procedure-body admission premises,
   so inference is outside that conditional soundness theorem's trusted base.
   - Inference starts from no summaries. Each round resolves every defined
     routine with one entry per parameter, and leaves an undefined routine
     unresolved (`infer_resolves_defined`). A call to an unresolved callee
     contributes no sink use in that round; this is sound only because
     elaboration refuses the call itself until the callee is resolved.
   - Rounds ascend (`owns_monotone`, `infer_monotone`, `infer_ascends`): no
     round withdraws a summary or turns a sink parameter back into a
     borrowed one. After the first round each later round can only flip
     entries from `borrow` to `sink`, so the iteration stops once two rounds
     agree, after at most one round more than the program has parameters,
     recursion included. That bound is argued here, not mechanized.
   - A nonconverged table is sound for bodies and calls that elaborate under
     it. `chain_needs_two_rounds` is a fixed admitted example with one extra
     copy; other bodies can be refused (`owned_update_needs_sink`). No general
     theorem proves that every body elaborates under the inferred/fixpoint
     table. Running inference to a fixpoint is an implementation requirement,
     not a proved general admission or minimum-copy result.
8. **Places (focus).** `focus t on y.p { s }` moves the part of the owned root
   `y` at path `p` into the temporary `t`, with exactly that part's blocks.
   The rest of `y` is suspended in the frame heap and is unreachable while
   `s` runs. Afterwards the final value of `t` moves back into the same
   place. No block is copied.
   - The body is elaborated with `t` live at its end and `y` removed from the
     live set. Among its refusal cases are when `y` is live inside the body, when
     `t` is still live after the focus, when `t` and `y` are the same
     variable, or when `y` is borrowed.
   - Handwritten core witnesses represent three intended source forms: an
     inout call on a member path (`AddDraw(inout state.draws, label)`), an update of a part from
     itself (`state.draws = Append(state.draws, label)`), and a read-only
     view of a part for a range that does not touch the root
     (`let items = state.draws` followed by reads of `items` only). A source
     selection/lowering function and its equivalence theorem are not supplied
     by those witnesses.
   - The mode table treats a focus as a write to `y`, so a parameter used as
     a focus root is inferred sink.
9. **Dead aggregates (unpack).** `unpack y into xs` moves each part of the
   owned record `y` into its own variable, with exactly that part's blocks,
   and frees only `y`'s own node block. No block is copied.
   - Among `elab`'s refusal cases are when `y` is live afterwards or borrowed,
     when `y` is also a part name, or when part names repeat or are borrowed.
     Parts that nothing reads are dropped right after the unpack.
   - It covers overlapping projections. `let items = h.items` while
     `h.name` is also read, followed by a use of `h` as a whole, becomes
     unpack, reads of the parts, and a pack of the parts back into `h`, with
     no copy (`overlapping_projection_copies_nothing`). The field-copy
     version pays two field copies.
   - Taking one part out of a record that is dead afterwards costs no copy
     (`dead_aggregate_part_moves`).
   - `unpack_after_pack` shows that unpacking a packed value gives each part
     back its original footprint.
10. **Regions.** `region x := f(ys) { s }` defines `x` at the start and keeps
    it live to the end of the region. The body is elaborated with `x` live
    at its end, so every use inside that consumes `x` copies it, and `x` is
    released once, on exit.
    - `region_released_at_exit`: no drop of `x` inside the region, one at
      its end.
    - Among `elab`'s refusal cases are a region whose value is still live
      after it, is redefined inside it, or is borrowed.
    - A value that escapes the region is copied out at the escape
      (`region_escape_copies`).
    - This models a string-concat region temporary and a zone-scoped value.
      `drops_commute` proves exchange of two adjacent drops of distinct
      variables. Arbitrary release-set permutations and reorderings involving
      borrow-end operations need their own derivation; they are not that
      theorem's statement.
11. **Exits** ([`proofs/OwnershipCleanExits.v`](proofs/OwnershipCleanExits.v)).
    `break`, `continue`, `return`, `throw`, and `try` are a layer over the
    core statements. Each exit has a target live set: the loop's
    continuation for `break`, the loop head for `continue`, the routine
    result for `return`, and the handler's live-in for `throw`. Elaboration
    uses the target set as the live-after set of the exit, so the settle
    before an exit releases exactly what the target does not need.
    - `xelab_sound`: every outcome (normal, break, continue, return, error)
      runs with the source trace and ends with exactly the target set bound,
      holding the source's contents. `x_closed_frees_everything`: a closed
      program frees everything on every outcome.
    - An error between building two parts and packing them releases both
      built parts on the way to the handler
      (`error_releases_partial_parts`). This is the model's partial
      initialization: a half-built value is never a value, only parts, and
      each part is released by its own owner.
    - `routine_exit_is_uniform` gives result-only correspondence for admitted
      normal/return executions; `xelab_sound` gives the general exit-target
      execution property. `early_return_elaborates` and `break_elaborates`
      only exhibit drop syntax in particular elaborated examples, not
      executions or edge-specific release proofs.
    - No exit edge needs a drop flag: the releases on each edge are
      unconditional, like those of a branch arm.
    - Routine-escaping errors and call unwinding are outside this exits
      layer. Callee packet recovery is covered separately in §5.10.5;
      lowering caller packet dispatch/propagation remains OPEN.

### 2.4 Proven properties

All execution claims below retain the source-execution, successful-elaboration
and invariant/correspondence premises of their theorem statements. GUI and
projection examples are handwritten core witnesses, not production lowering
proofs or installed GUI executions.

| Theorem | Meaning |
|---|---|
| `elab_sound` | For every mode table, and every function table whose bodies elaborate under it, the elaborated program runs with no refused step and emits exactly the source trace. At every statement boundary the bound variables are exactly the live ones, holding the source's contents. |
| `heap_is_live_footprint` | At every boundary the live heap is a permutation of the owned footprints plus the frame heap: no block is shared (no alias, no double free) and none is unowned (garbage free). |
| `closed_program_frees_everything` | A closed program ends with an empty heap and empty environments. |
| `elab_copy_moves_dead_source`, `elab_copy_copies_live_source`, `elab_copy_copies_borrowed_source` | The move/copy decision table. A copy is emitted only when the source is still live or only borrowed. |
| `gui_program_runs_clean` | The `Array<GuiDraw>` + `Label(String)` shape that blocked the GUI work runs with zero copies and frees everything. `gui_program_reuse_copies_once` shows that reading the label after the push costs exactly one copy. |
| `gui_calls_sink_moves_only`, `gui_calls_sink_runs_clean` | The same shape through a constructor function and an inout append procedure. With inferred modes it copies nothing and frees everything. With borrow-only modes it copies twice (`gui_calls_copy_twice`, `gui_calls_runs_clean`). |
| `gui_calls_sink_reuse_copies_once` | A caller that still needs the text after the constructor call pays one copy, at the call site. Borrow-only modes pay two, inside the callees (`gui_calls_reuse_borrowed_copies_twice`). |
| `owned_update_needs_sink` | `Append(xs, y)`, which pushes `y` and returns `xs`, is refused with borrow-only modes. Once both parameters are inferred sink it runs with zero copies and no annotation. |
| `gui_state_focus_copies_nothing`, `gui_state_runs_clean` | A `GuiState` record with `AddDraw(inout state.draws, label)` on its member path: no copy and no field copy, and everything is freed. |
| `gui_state_detach_copies_the_field` | The same update with today's detach/restore ceremony costs one field copy. |
| `gui_state_update_copies_nothing` | `state.draws = Append(state.draws, label)` through a sink `Append`: no copy. |
| `projection_view_copies_nothing` | A read-only view of `state.draws` through a focus costs nothing; binding it to a local with a field read costs one field copy. |
| `focus_refusals` | Four focus-refusal examples, not an exhaustive admission specification. |
| `pack_child_segment` | For a direct child at path `[i]`, immediately after pack, the computed block segment equals that child's input footprint under the size premises. Not a general nested-path or physical-layout theorem. |
| `gui_modes_inferred`, `gui_modes_fixpoint` | Inference marks both GUI parameters sink, and one more round leaves the table unchanged. |
| `settle_tail`, `elab_def_releases_operands`, `elab_call_releases_operands` | The releases after a statement depend only on the variables it mentions. An implementation tests membership once per operand instead of walking the live set. |
| `alias_copy_double_free` | A shallow alias copy (today's descriptor copy, the open C2 case) makes the two automatic drops free one block twice. The second free is refused. |
| `early_drop_use_after_free` | Dropping before the last use leaves that use stuck. |
| `borrowed_drop_refused` | A callee cannot free a borrowed parameter. |
| `missing_settle_leaks` | Without `settle`, the dead definition's binding still owns a live block; it is not an unowned block. |
| `overlapping_projection_copies_nothing`, `dead_aggregate_part_moves`, `unpack_after_pack`, `unpack_refusals` | Fixed unpack examples: overlapping part reads and a part taken from a dead record copy nothing; packed parts recover their footprints; four refusal witnesses, not an exhaustive list. |
| `rec_unpack_runs_clean` | The unpack-read-repack program runs with no refused step and frees everything. |
| `region_released_at_exit` | Successful elaboration puts the region variable's drop at the exit, not in its body; this is a syntactic placement result. Execution safety is inherited from `elab_sound`. |
| `region_value_released_once`, `region_escape_copies`, `region_refusals` | Fixed examples with zero copies, one escape copy and three refusals. Despite its name, `region_value_released_once` states no release/execution property. |
| `no_drop_live` | No statement drops a variable that is live after it and that it does not write. |
| `xelab_sound`, `x_closed_frees_everything` | Exits: each outcome runs with the source trace and ends with its target live set; a closed program frees everything on every outcome. |
| `error_releases_partial_parts`, `routine_exit_is_uniform` | Partial-parts error witness and admitted normal/return result-only correspondence, respectively. |
| `early_return_elaborates`, `break_elaborates` | Drop-syntax existence in two fixed elaborated examples; use `xelab_sound` for the general execution claim. |
| `missing_summary_refuses_call`, `arity_mismatch_refuses_call`, `missing_summary_refuses_routine`, `arity_mismatch_refuses_routine`, `missing_summary_is_refused`, `arity_mismatch_is_refused` | No summary, or a summary of the wrong length, is a refusal at the call and at the routine. |
| `owns_monotone`, `infer_monotone`, `infer_ascends`, `infer_resolves_defined` | Inference rounds from no summaries ascend; every defined routine is resolved with the right length after one round, and an undefined one stays unresolved. |
| `chain_needs_two_rounds` | A call chain converges in two rounds; the unconverged table costs one copy and is still admitted and sound. |

Two consequences matter for the implementation:

- **The model's C2 alias case is excluded by construction.** A copy is either a move or a fresh deep copy,
  so a shared backing never exists. The aliasing questions that the
  aggregate-release analyzer tries to answer site by site no longer arise
  in that model. Production storage/glue/consumer migration remains OPEN.
- **No drop flags.** ASAP placement plus per-arm settling makes every
  release unconditional at its program point. This is a property of this
  structured cleanup model, not a general account of another language's drop
  strategy. It is proven for the structured statements and for
  `break`, `continue`, `return`, `throw`, and `try` (step 11). Partial
  initialization is a sequence of part definitions followed by a pack, so an
  error before the pack releases the parts.
- **Linear release placement.** `settle_tail` shows that the variables
  released after a statement are among the ones it mentions. Straight-line
  code costs one membership test per operand. Only branch arms, loop
  boundaries, and routine entry compare whole live sets, which a bitset does
  in one pass. The quadratic-to-cubic cost of the extracted proof model comes
  from its list-based sets, not from the algorithm.

### 2.5 Negative scope of the core

- The semantics is big-step, so divergent runs are not covered. Each
  terminating prefix of a long-running event loop is a separate finite run;
  the theorem says nothing about peak memory or OOM.
- Exits are a layer, not core statements, and an abort is not an exit. A
  panic that ends the process runs no release; the exits layer covers only
  its structured outcomes. Routine-escaping errors, `?` propagation across a
  call and unwinding are not supplied by `XThrow`/`XTry`; caller packet
  decoding/handler lowering is still OPEN even after the CL6 callee bridge.
- A part of a borrowed root, read into a variable, is still a fresh copy:
  unpack and focus both need an owned root. A focus covers the in-place
  cases and unpack the overlapping ones.
- A focus path addresses children of nodes. Map keys, set members, and
  array elements at a runtime index are paths whose index is a runtime
  value; the core takes the path as given, so the implementation must check
  the index before the focus and fail closed on a missing element.
- A drop has no observable effect. A resource whose release is visible (a
  file, a socket, a zone handle) keeps its own cleanup edge (section 3).
- Elaboration refuses a call through an unresolved summary and any summary
  of the wrong length (step 6). It does not check types, and the theorems
  take the source execution as a premise. The implementation reads name,
  type, and arity facts from semantic admission and fails closed when one is
  missing (section 5).
- Soundness for any admitted mode table and monotone inference do not prove
  general elaboration success under the inferred table. GUI/fixed-chain
  admission examples do not discharge that adequacy obligation.
- A region holds one value; nested regions give several. Slot, async, and
  FFI are outside the model, and so is the physical allocator: the model's
  abstract block allocator is not refined against `pgy_alloc`. Recursion is
  covered, because big-step derivations are finite.
- Write-through Slice and returned views are outside this independent-value
  source core. Checked loan/current-lifetime refinement or fail-closed
  exclusion of the affected automatic-elaboration boundary is required
  (§5.10); exclusion means compilation refusal, not omitted cleanup. The
  two-field Slice descriptor has no generation issuer identified yet.
- Multi-inout plus an independent result and ordered expression/short-circuit
  normalization are not proved by the single-output `SCallIO` (§5.10).

### 2.6 Composition laws and read-only alias elision (adopted)

Adopted on 2026-10-08 after review. The design is the one in
[`../207_compiler_owned_cleanup_algorithm.md`](../207_compiler_owned_cleanup_algorithm.md)
sections 9 and 10. Two supplementary proofs import the core and reuse its
source and target semantics. Neither adds a heap, an ownership rule, or
syntax. Rocq 9.3.0 and `rocqchk` check both, with no assumptions.

**Composition** ([`proofs/OwnershipCleanComposition.v`](proofs/OwnershipCleanComposition.v)).
Sequencing in the ownership machine behaves like bind in a state-and-trace
monad whose result is unit. `cleanup_equiv` is exact equivalence of target
executions from every start state: the same trace, environments, heap, and
allocation frontier.

- Laws: left and right unit, associativity, congruence for sequence, branch,
  and loop, and branch-bind,
  `(if c then a else b); k  ≈  if c then (a; k) else (b; k)`.
- `normalize_cleanup` removes administrative `Skip` nodes with these laws,
  and `elab_normalized_sound` carries the soundness theorem over to the
  normalized output.
- Falsifiers on the same machine:
  - a branch with empty arms still reads its guard;
  - a drop in both arms runs once, while two drops in sequence are refused;
  - two observations are not one.

These laws are the equational theory for every target-level rewrite. C3
admits a rewrite of target code only with a `cleanup_equiv` proof, never by
comparing program output.

**Read-only alias elision** ([`proofs/OwnershipCleanReadOnly.v`](proofs/OwnershipCleanReadOnly.v)).
Given `a := x` followed by a region, the rewrite replaces reads of `a` with
reads of `x` and deletes the copy, then runs the canonical `elab` on the
result. This is copy propagation on the value-semantics source. No pointer,
loan, or second owner exists: the root's liveness extends to the last former
use of `a`, so the root is released after it.

- `readonly_copy_elision`: an admitted rewrite runs with the same trace and
  the same final values, except for `a`.
- `readonly_elaboration_frees_everything`: the rewritten closed program frees
  everything.
- `ro_demo_one_fewer_allocation`: on a fixed program, one fewer abstract
  allocation, proven on both executions.
- `ro_mutation_changes_observation`: a write to the root inside the region
  would change the observed value, so the rewrite refuses it.
- D1 applies after the rewrite, to the copies that remain. Applying it to the
  original copy first would refuse legal elisions.

**Why substitution and not a loan.** A borrow binding (`a` lends the storage
of `x` and pins `x`) needs new frame state and a pin check in every
consuming rule. Under value semantics an alias is unobservable except through
writes, so substitution reaches the same result with no runtime state and no
change to the machine. This statement concerns ordinary independent values,
not the runtime's existing write-through `Slice` views or indexed String
borrows. Those require backing-owner lifetime facts (§5.10); substitution
must not change a mutable view into an independent value.

**Refinements.** Both are in the repository proofs and checked under Rocq
9.3.0 and `rocqchk` with no assumptions. The scratch module where they were
first checked is kept at
`.tmp/ownership-cleanup/claude-refinements/OwnershipCleanWide.v`.

1. **Admission.** The current `ro_region` admits copy, pack, push and calls
   when their destination (or inout target) is neither `a` nor `x`. It
   recursively checks sequences, branches and loops, and refuses focus,
   unpack and region statements. `ro_admit` additionally requires distinct
   names, an alias absent from the live-out set and an alias not in the
   borrowed set. `elab_readonly_program` preserves original canonical
   admission before rewriting. These are sufficient bounded checks, not
   the claim that one no-write condition handles every source construct.
   - An overlap that contains a call is then accepted (`ro_call_in_overlap`,
     `ro_value_semantics_accepts`).
   - If the root is still needed after a sink use, the copy moves to the
     call site in that example. The fixed demo and `ro_call_in_overlap`
     establish their copy counts; a general copy-count nonincrease theorem
     is not present.
2. **Drop order.** `drops_commute` (`OwnershipCleanComposition.v`) shows
   `cleanup_equiv` for exchanging two adjacent drops of distinct variables.
   General permutation and borrow-end motion are
   further proof obligations before arbitrary release scheduling.

**Still open.**

- A projection alias such as `let items = h.items` is a focus when the range
  of `items` does not use `h` (section 2.3, step 8), and an unpack when the
  range also reads another part of `h` (step 9) in the handwritten witnesses.
  Automatic selection and source-to-core equivalence remain OPEN; these
  witnesses do not classify every projection program.
- In SSA MIR, an SSA value is never redefined. The production check therefore
  can avoid redefinition checks, but eliminating the other admission guards
  requires a production refinement. No such one-rule theorem is proved here.

## 3. Language rules that follow

1. **One owner per heap value.** The owner is a local, a field, an element, or
   a compiler temporary. Users never write release calls for ordinary values.
2. **The compiler derives ownership facts.** Each inserted copy, move, and
   drop is a compiler fact recorded in IR and reported by a diagnostic
   observer. It is not source syntax. (AGENTS.md: derived choices must remain
   inspectable.)
3. **Parameters.** A parameter the callee only reads is borrowed: no copy,
   no release in the callee. A parameter the callee stores, mutates, or
   returns is sink: the caller moves the argument at its last use and copies
   it only if it is still needed. Both are inferred facts, not annotations
   as the intended language contract. Section 2.4 proves soundness for
   admitted mode tables and monotone inference, with fixed GUI/chain
   admission examples; general inferred-table admission remains OPEN.
   `inout` is an exclusive move-in/move-out. On a
   member path it is a focus on that path (section 2.3, step 8), so the
   other arguments cannot mention the root.
   - Under D1 (B), passing an explicit-copy value (a collection) to a sink
     parameter while the caller still needs it requires `Clone(x)` at the
     call site. The diagnostic names the sink parameter that stores the value
     and the later use that keeps the source alive. At the last use the value
     moves, with no annotation.
   - Inferred modes are part of each function's derived summary, recorded in
     MIR and shown by the observer (section 5). A body change that turns a
     parameter sink can make a caller need `Clone`; the diagnostic says why.
   - A callee without a summary is refused (`OWN_MISSING_CALLEE_SUMMARY`);
     it is never assumed all-borrowed. An `extern` function's summary is part
     of its ABI declaration: trivial parameters need none, and a heap-typed
     parameter needs a lend or consume row, as `ForeignStringOwnership.v`
     gives for strings. A function value's type fixes its summary as
     all-borrowed, and converting a named routine into a value checks that
     its inferred summary is all-borrowed
     (`OWN_FUNCTION_VALUE_SUMMARY_MISMATCH` otherwise).
4. **Existing safety boundaries stay.**
   - Slot keeps its generation checks.
   - Zone, world, effect, and intent handles keep their own cleanup edges.
   - `async`/`parallel`/worker boundaries accept only a move or an explicit
     copy (AGENTS.md worker rule).
   - FFI strings keep `ForeignStringOwnership.v`.

## 4. Decisions and next cores

**D1, copy policy: decided (B)** by the user on 2026-10-08. The model inserts a
copy whenever the source is still live. Any policy that refuses some of those
copies is compatible with the proof, because refusing a program only shrinks
the set that elaboration accepts. The options were:

- (A) Implicit copy for every type.
- (B) Implicit copy for String and plain aggregates; `Clone` for collections.
- (C) `Clone` for every non-trivial copy.

Under (B), every type falls into one of three classes:

| Class | Types | Copy of a still-live source |
|---|---|---|
| Trivial | Int, Long, Float, Double, Bool, Duration, payload-free enums, and Option/Result/struct/enum built only from trivial parts | Bitwise. No drop. |
| Implicit-copy | String, and Option/Result/struct/enum built from trivial or implicit-copy parts | The compiler inserts a deep copy and records a copy fact. |
| Explicit-copy | Array, List, Queue, Set, HashMap, and any aggregate that contains one | Refused unless written `Clone(x)`. The diagnostic names the later use that keeps the source alive. |

Affine handles (Slot, subject, Future, zone/world/effect handles) are not
copyable at all, as today.

`Slice<E>` is an existing borrowed view, not an independently owned collection
in this D1 table. It neither owns nor drops the array backing. Copying its
descriptor cannot mint a cleanup obligation; writes remain writes through to
the same admitted backing. Its backing/place and checked current-lifetime
facts must reach liveness; the generation/lifetime issuer is still OPEN
(§5.10), not an already existing descriptor field.

`Clone` must deep-copy nested collections. Today `Array<String>` clone
already copies each string, but nested-array clone copies inner descriptors
by value (section 5.4), and G3 fixes that.

Before an explicit-copy refusal, the compiler tries two cheaper outcomes:

1. **Move at last use.** If the source is not used afterwards, there is no
   copy.
2. **Read-only alias elision.** If the rewrite's admission checks (§2.6)
   succeed, reads of the alias become reads of the original, and its release
   follows the substituted uses. The alias-definition allocation disappears;
   consumer copies may remain and are still subject to D1. This is not a
   general zero-allocation or no-`Clone` promise for every admitted region.

`Clone` is therefore demanded only where a real independent copy is
observable. Elision is an optimization of the value semantics, proven for
whole-value aliases in section 2.6. A projection alias uses a focus or an
unpack (section 2.3, steps 8 and 9).

Alternatives checked:

- Copy-on-write and persistent collections need reference counting or a
  collector, which `AGENTS.md` excludes.
- Making every non-trivial copy explicit (C) adds ceremony for Strings, which
  are cheap and immutable.
- Mojo's 2025 standard library moved its collections (`List`, `Dict`, `Set`,
  `Deque`, ...) from implicit to explicit copy and kept implicit copy for cheap
  values. That is the same split as (B).
- String copies cost O(n). A small-string representation is a runtime option
  that leaves the semantics unchanged.

**Sink inference: fixed model witnesses proved; general adequacy OPEN.**
`gui_calls_sink_moves_only` removes both copies of
`gui_calls_copy_twice`. Under (B) this was required for DX: without it every
constructor that stores a collection parameter would demand `Clone`.

**Read-only alias elision and handwritten places: bounded model evidence**
(sections 2.3 step 8 and 2.6), not production selection/lowering closure.

**Overlapping projections, exits, and regions: bounded core evidence** (section 2.3, steps 9
to 11), with fail-closed summaries and ascending inference (steps 6 and 7).

**Not modelled, and the hard part of the implementation.** The proofs fix
what the pass must decide. These obligations are the implementation's, and
no theorem here discharges them:

- member-path identity in MIR: the path of a focus or unpack is static in
  the model, while a runtime index must be checked before the access and
  fail closed on a missing element (section 2.5);
- the interprocedural fixpoint over the real call graph, with summaries
  recorded per routine and an unresolved summary kept distinct from a
  borrowed one;
- per-type drop and copy glue in the runtime, deep for every copy
  (section 5.4);
- the physical allocator: the abstract block allocator against `pgy_alloc`
  and its arenas;
- panic and abort, divergent runs, and peak memory;
- async, `parallel`, worker boundaries, Slot, and FFI beyond strings.
- the §5.10 production bridge: ordered multi-inout/result and short-circuit
  source normalization, loan/view lifetime certificates, a declared
  collection/String ABI owner, and mutation/continuing-failure postconditions.
  These require checked refinement or importing proof supplements, not only
  implementing the existing target elaborator.

**Implementation order.** Removing manual retirement before automatic release
exists would exceed the 3 GiB pressure cap, and dropping before the storage
model changes double-frees, so the order is:

0. Replace the storage model: one owner per backing, and deep copy glue for
   every copy a fact asks for, with today's shallow paths removed
   (section 5.4). No drop is emitted for a type before its copies are deep.
1. Native MIR drop elaboration and per-type drop glue, consumed identically by
   C and LLVM.
2. The same elaboration in the self-hosted compiler; gen2 == gen3 must hold.
3. Delete the manual release calls in compiler and user code.
4. Delete the aggregate-release analyzer, carrier/restoration, and
   own-threading paths, with negative gates against their return.

The runtime and backend work the model assumes (String storage classes,
per-type drop and copy glue, missing collection drops) is specified in
section 5.4.

## 5. MIR ownership contract (C2)

This section is the only interface between the ownership-clean pass and its
consumers: the C emitter, the LLVM emitter, the runtime, and the self-hosted
emitter (work items C3, C5, G3, G4). A consumer reads these facts and
nothing else. It never decides a move, copy, or release on its own, and it
refuses MIR that lacks a fact it needs.

Contract identity: `pgy.mir.ownership.v1`.

### 5.1 Objective card

- **Objective:** every heap value is released exactly once, at its last use,
  by code the compiler derives; program output is unchanged.
- **Priority:** first refinement of `OwnershipCleanCore`, then fail-closed
  admission, then C/LLVM parity, then deletion of the manual paths.
- **Fact owner:** the ownership-clean owner enters after existing MIR DCE,
  performs its ordered source normalization/copy propagation and other
  selections, then admits facts from checked final-generation analysis before
  `mir_validate` and emission (`src/compiler/mir.c`, `driver_app.c`). Whether
  DCE runs again after normalization is a P1 decision; if it does, analysis
  must be refreshed afterwards. `mir_lower` currently runs DCE after its
  initial analysis, so that initial certificate is not the pass's final one.
  The self-hosted
  compiler runs the same pass (C5) and must produce the same facts, so
  gen2 == gen3 holds.
- **Last consumers:** the C emitter (`transpile_from_mir`,
  `src/codegen/transpiler_entry.c`), the LLVM emitter (`llvm_codegen_from_mir`,
  `src/codegen/llvm_api.c`), the self-host emitters, and the ownership
  observer.
- **Registration:** the facts are one fact family. At C3, following
  `docs/180`, the implementer either adds a new row to
  `docs/semantics/sot_owner_spine_registry.md` or makes the family the
  successor identity of `semantic.hashmap_collection_ownership`
  (registry line 110). The MIR JSON change is a row in
  `docs/192_protocol_abi_api_registry.md`. That registry rejects unknown
  fields, so every producer and every JSON consumer (`mir_lower`,
  `direct_mir_*`) changes in the same slice.
- **Forbidden:**
  - a backend that decides a move, copy, or release itself;
  - reading ordinary-value cleanup ownership from `own` annotations, from the
    `ast_collection_aggregate_*` analyzer, or from the AST outside the
    instruction payloads MIR already carries;
  - a fallback that skips a drop when a fact is missing;
  - reference counting, tracing, or a "leak instead" mode;
  - a shallow copy where a fact says copy;
  - a second cleanup authority. These become old paths, deleted once their
    consumers migrate:
    - the per-instruction `collection_ownership_receipt` state machine
      (`src/compiler/mir_branch_source_facts.c`; exact span is a P0 inventory,
      not the older snapshot's line numbers);
    - `semantic_collection_admit_owned_string_drop`
      (`src/semantic/collection_ownership_fact.c`);
    - the self-host direct-MIR cleanup policy owners
      (`direct_mir_scalar_program_array_string_cleanup_policy_owner.pgy`,
      the HashMap lifetime and cleanup owners);
    - the `ast_collection_aggregate_*` analyzer.

### 5.2 Inputs

For each routine the pass reads, from admitted MIR:

- the routine identity, its parameters in order, each parameter's type, and
  its source mode (`inout`, `ref`, or neither);
- every SSA value of every local and temporary, with its type and its D1
  class: trivial, implicit-copy, explicit-copy, affine, or a resource with
  its own cleanup edge;
- for every call, the resolved callee identity (or `extern`, or a function
  value) and its arity;
- for every runtime call, a row from its declared/admitted ABI fact owner.
  Existing resource rows cover Slot operations; the current
  `CompilerRuntimeValueCallAbiFact` is an Allocator/TextBuilder join with up
  to two parameters, not an owner of all collection/String calls. P1 must
  declare the collection/String owner/registry identity before its new
  metadata is consumed. Rows state per argument lend/consume/reset, result
  storage and ownership origin (fresh, transferred, argument/part view, no
  result), and mutation/continuing-failure postconditions. A missing owner or
  required column refuses; storage class alone is not ownership;
- for every borrowed view, the backing owner/place and a checked current
  lifetime certificate. A view use keeps its backing owner live. Its existing
  generation/certificate producer is UNKNOWN; the two-field Slice descriptor
  does not carry one. P1 must resolve the issuer/refinement or refuse affected
  automatic elaboration, never substitute a raw pointer/source ID;
- the block structure: successors, including the cleanup, rollback, and
  invalidation successors, and the block `live_in`/`live_out` sets that
  `mir_recompute_analysis` already computes.

Each fact comes from its existing owner: semantic admission, the MIR
builder, or the declared runtime-call ABI owner (including P1's still-OPEN
collection/String owner). A missing fact is a refusal
(`OWN_MISSING_ADMISSION_FACT`). Native analysis is refreshed after a def/use
rewrite; the self-host requires its first liveness producer, not a guessed
empty set or the historical manual release policy. Ownership elaboration
consumes/checks the final-generation certificate, as `elab` checks a loop head,
and does not independently reconstruct a competing liveness authority.

Source modes:

- `inout` stays the source's value-result form.
- `ref` stays an explicit readonly boundary, and its parameter is borrowed.
- Every other **ordinary-value** parameter, including one written `own`, gets
  its mode from inference. For those values `own` does not change a decision,
  the observer reports a
  mismatch, and G5 removes the annotations.
- Affine/authority transfer and named-boundary requirements are not removed
  by this ordinary-value rule. Slot, subject and observable handles retain
  their existing contracts.

### 5.3 Facts and the MIR-to-core mapping

MIR is a CFG in SSA form. Its operands are expression payloads (`expr0`,
`expr1`) of DEF, ASSIGN, STMT, and RETURN instructions
(`src/compiler/mir_types.h:81-248`); there is no separate call, copy, or
move instruction. The contract therefore keys every fact by routine, block,
instruction index, and the operand's syntax node identity.

**Model variables.**

- Each SSA value of a heap-typed local is one variable of the model.
- A PHI is a move on each incoming edge: the incoming value moves into the
  PHI result.
- Every unbound heap-valued subexpression, such as the result of `g(x)` in
  `f(g(x), y)` or a heap `StringConcat`, is a compiler temporary. It is
  either consumed by its consuming position or dropped right after the
  instruction. This is the model's `SDef` of a temporary followed by
  `settle`.

The pass emits three kinds of facts.

1. **Routine summary.** `param_modes` gives `borrow` or `sink` for each
   parameter, and each sink entry carries its reason. An inout parameter is
   `sink` and has a required ownership recovery output; the bounded core has
   only one such output. Production multi-inout calls require the checked
   normalization in §5.10. The pass iterates mode
   inference over the call graph to a fixpoint (section 2.3, step 7), then
   checks every routine under the final table. A routine whose summary is
   not computed has no `param_modes` row; that absence is not
   "all-borrowed", and a consumer that meets it refuses.
2. **Operand decisions.** Each operand position gets one of:
   - `lend`: a borrowed read; no copy, no transfer;
   - `move`: the descriptor is transferred and the source value is dead;
   - `copy(glue)`: a fresh deep copy;
   - `copy_to_temp(glue)` followed by `move`, for a consuming position whose
     source is still needed.
3. **Releases.**
   - `drop(value, glue)` after the instruction where `value` dies;
   - `drop_on_edge(edge, values)` for each value in `live_out(pred)` that is
     neither in `live_in(succ)` nor moved into a PHI on that edge. This is
     the model's per-arm settle and loop entry/exit settle on a CFG. Cleanup,
     rollback, invalidation, and return edges are included.
   - `end(value)` for a borrowed binding that dies. It emits no code and
     exists for the observer.

Each normalized statement maps to one core statement, and its facts must
equal what `elab` produces. An existing MIR instruction with nested expression
payloads may need an ordered sequence of normalized statements/temporaries;
block liveness of its original aggregate use set does not prove operand-level
last-use order. The normalizer records those positions using the existing
typed expression identities (§5.10):

| MIR instruction and payload | Core statement | Decision |
|---|---|---|
| DEF or ASSIGN `x = op(ys)` (operator, comparison, pure runtime read) | `SDef` | `lend` each operand; `x` is fresh |
| DEF or ASSIGN `x = y` | `SCopy` | `move` if `y` is dead and owned; otherwise `copy`, subject to D1 |
| DEF or ASSIGN `x =` a struct literal, array literal, or enum variant with payload | `SPack` | each element is consumed: `move`, or `copy_to_temp` then `move` |
| STMT runtime insert (`ArrayPush(xs, y)` and the like) | `SPush` | each argument as its ABI row says |
| DEF or ASSIGN `x = y.f` or `x = y[i]` | `SField` | `x` is a fresh copy of the part, subject to D1 |
| a `member_access` or `index` used directly as an operand | a read of `y` | `lend` `y`; no copy |
| a call in DEF, ASSIGN, or STMT | `SCall` | `lend` borrowed arguments; consume sink arguments |
| a normalized call with one inout recovery output | `SCallIO` | `z` moves in and moves back; multi-inout and a separate result require §5.10 normalization |
| a BRANCH (expr, match-case, for-range, for-in, select) | `SIf`, `SWhile` | edge drops as above; the loop header's `live_in` is the certificate |
| RETURN `e` | `XReturn` (exits layer) | `e` moves to the result; every other live owned value drops on the return edge |
| `break`, `continue` | `XBreak`, `XContinue` | the values outside the target live set (the loop's continuation, or the loop head) drop on the exit edge |
| an error handled within the structured body | `XThrow` inside `XTry` | the values outside the handler's live-in drop on this local error edge, including parts built before a pack |
| routine-escaping `?`, failed `Result` return or call error propagation | callee outcome packet (§5.10.5); caller dispatch lowering OPEN | recover all inouts before interpreting/propagating the packet; `XThrow`/`XTry` alone do not prove this cross-call lowering |
| DEF `x = y.f` while another part of `y` is read, `y` otherwise dead or used only whole afterwards | `SUnpack y xs` ... `SPack y xs` | each part moves into its own local; a whole use repacks them; no copy |
| a string-concat region temporary, a zone-scoped value | `SRegion` | the value stays live to the region's end and drops there; an escape is copied out |
| ASSIGN to a place from itself, `y.f = g(y.f, args)` | `SFocus t y [f] (t := g(t, args))` | the part moves out, through the sink call, and back; no copy |
| a call with an inout member path, `g(inout y.f, args)` | `SFocus t y [f] (g(inout t, args))` | the part moves in and back; the arguments must not mention `y` |
| ASSIGN to a place from another value, `y.f = e` | `SFocus t y [f] (t := e)` | the old part is dropped at the focus entry; `e` is consumed |
| DEF `x = y.f` whose range reads only `x` | `SFocus x y [f] (range)` | a read-only view; no copy |
| `async`, `parallel`, or worker capture | not in the core | `move` or an explicit copy only |

**Calling convention.** The bounded single-inout model does not require a
public ABI change. The production multi-output adapter must still be specified
and checked (§5.10); no product-result representation is inferred merely from
this statement. Today a default parameter
is passed by value, which is a shallow descriptor copy, and `inout` is a
pointer with copy-in and write-back
(`src/codegen/transpiler_func_forward_emit.c:185-245`,
`src/codegen/transpiler_defer_emit.c:142-178`). A sink parameter and a
borrowed parameter are both passed by value; they differ only in who drops
the value. For a sink argument, the callee drops it and the caller does not.
For a borrowed argument, the caller keeps it and the callee never drops it.

**Pass order and owner.** The ownership-clean owner coordinates ordered
source expression/place/call normalization, then copy propagation (section
2.6), then mode inference, focus/unpack selection and final-generation
analysis checks, then target elaboration.
Any later change to def/use invalidates the old liveness certificate and all
dependent facts. D1 is checked last, on the copies that remain.

**Focus selection.** The pass chooses a focus for a part access when the
root is owned and the range of the temporary does not mention the root. The
range is the source statement for an update or an inout call, and the
alias's live range for a read-only binding. A focus is a fact like any other
(section 5.7). The pass checks it, as `elab` does, and refuses instead of
aliasing when the check fails.

**Projection selection.** A whole-value alias (`let a = x`) is removed by
copy propagation, and the three place forms above use a focus. A part bound
to a local whose range also reads another part of the root, for example
`let items = h.items` followed by a read of `h.name`, uses an unpack. The
pass reports `OWN_EXPLICIT_COPY_REQUIRED` for a collection-typed part only
when the root is borrowed, or when the part is a collection element at a
runtime index while the collection is also read.

### 5.4 Drop and copy glue

Each type has one drop descriptor and one copy descriptor. The type owner
generates each descriptor once, and C and LLVM consume the same descriptor.

**Order.** For a type, deep copy glue and the removal of its shallow copy
paths land before any drop of that type is emitted. A drop on a shared
backing is a double free (`alias_copy_double_free`), so a drop without the
storage change is refused in review, not staged.

| Type | Drop | Copy (only where a fact says copy) |
|---|---|---|
| Trivial (D1 class) | none; no drop is emitted | bitwise |
| `String` | free the buffer if the value is heap storage (below) | allocate and copy the bytes; the result is heap storage |
| `Array`, `List`, `Queue`, `Set` of `E` | drop each element with `E`'s glue, then free the backing | new backing; copy each element with `E`'s copy glue |
| `HashMap<K, V>` | drop each key and value, then the table | copy every entry |
| struct | drop each field, in declaration order | copy each field |
| enum, `Option`, `Result` | drop the active variant's payload only | copy the active payload |
| `Slice<E>` / admitted borrowed view | end the view; never free its backing | view descriptor only under its backing-owner lifetime certificate; not an owning deep copy |
| Affine handle (Slot, subject, Future, zone/world/effect handle) | never through this glue; the handle keeps its own cleanup edge | refused |

Rules:

- Copy glue is deep and allocates one block per node, as `TE_Copy` does. It
  is the refinement of `TE_Copy`, which gives the copy fresh storage.
- A value's storage is laid out so that each part's blocks can be found from
  its path (one block per node, in preorder, in the model). A focus moves
  that modelled segment; `pack_child_segment` identifies the input footprint
  for a direct child `[i]` immediately after pack, not general nested runtime
  layout. The required runtime refinement means a field's
  descriptor is moved out of and back into its slot by a bitwise move, and
  no other storage moves with it. A copy that shares any backing with its source breaks the
  contract; `alias_copy_double_free` is the failure it causes.
- Drop glue releases exactly the value's footprint. It is the refinement of
  `TE_Drop`. Freeing an element twice, or leaving one behind, breaks the
  contract.
- A move transfers the descriptor bitwise and nothing else. The source is not
  dropped; the destination owns the value.
- Runtime gaps that G3 closes:
  - There is no per-type drop glue today.
  - `List`, `Set`, and `Queue` have no whole-collection drop function.
  - `HashMap` release paths exist (`pgy_map_drop_*` and the recorded CLOSED
    `abi.hashmap_runtime_release` family), including String-value payload
    release. This is not evidence of general ownership-fact-directed automatic
    emission. Exact existing backend reachability is still to be checked;
    do not claim that no backend can emit a release at all.
  - The exported `pgy_array_drop_owned_String` calls `free(arr->data)` and
    ignores the allocator, while the inline path uses `pgy_free` with the
    allocator. Glue must use the value's allocator.
  - Two shallow paths must become deep copy glue:
    - the LLVM nested array literal shares the inner `data` pointer
      (`src/codegen/llvm_expr_aggregate.c`; old line citations refer to
      different snapshots; P0 rebinds the exact span);
    - nested-array clone copies each inner descriptor by value
      (`src/runtime/pgy_runtime_lib_array_nested_exports.h:16-21`).
  - `Array<String>` clone already duplicates each string
    (`src/runtime/pgy_runtime_memory_array_slot_inline.h:197`).
- Runtime inserts disagree on copying. `ArrayPush` stores the `String`
  pointer it is given, while `Queue<String>` and `Map<String>` copy on insert.
  The runtime-call ABI row records each case: a copying insert lends its
  argument, and a storing insert consumes it. That statement only applies
  where both backends already satisfy the same operation postcondition. A
  C-copy/LLVM-alias disagreement, such as String set-values, must be unified
  in I1 before one ABI semantic row can admit it. It cannot be hidden as two
  backend-specific ownership decisions or by recording both lend and consume.

Whole-value glue is not the full mutation contract. Replace/set must consume
the old payload exactly once; pop/discard/remove/clear must either transfer a
removed payload to a named result or drop it. Growth relocates admitted owned
descriptors without duplicating ownership; a copy operation deep-copies them.
Duplicate insertion and failure must say whether an incoming operand was
consumed, and release only constructed parts on a continuing/recoverable
failure. Terminal panic/abort is outside the core's exit proof. These postconditions
belong to the same type/operation ABI owner and are consumed identically by C
and LLVM. Walking only the final length at collection teardown cannot recover
an overwritten or previously removed payload (§5.10).

**String storage classes.** A runtime `String` is a `char *` today and does
not record whether it may be freed. A `String` value has one of three
storage classes:

| Class | Produced by | Dropped |
|---|---|---|
| static | a literal | never |
| region | a string-concat temporary inside its region | by the region's bulk release, never individually |
| heap | a runtime call that returns fresh storage, or a copy | by drop glue |

These are storage classes, not a grant of ownership. A getter can return a
pointer into an argument's heap storage without transferring that argument's
ownership. Its ABI result-origin fact names the backing owner and part. The
normalizer must materialize a fresh value copy, an admitted dead-part move, or
a checked view range as appropriate; it never independently drops that pointer
because its storage class is heap. A missing origin is a refusal.

The pass knows each producer's class: literals are static, and the
runtime-call ABI owner declares, for every call that returns a `String`,
whether the result is heap or region storage. A missing declaration is a
refusal. A static or region value behaves like a borrowed binding in the
model. Reading it is free, and moving it into owned storage (a sink
parameter, a field, a collection element, a variable that is later dropped)
inserts a copy, so the owned value is heap storage. Where control flow joins
values of different classes into one variable, the non-heap arm is copied
before the join, so every variable has one class. This keeps `String` a
`char *`, so the FFI contract (`ForeignStringOwnership.v`) is unchanged. G3
may replace it with a runtime owned flag only if it measures the cost and
keeps the same rules.

### 5.5 Migration of manual releases

This is the historical ABI classification used to migrate existing calls,
not an accepted public mixed manual/automatic mode. The cutover lands only
after admitted ordinary-value programs contain no manual releases. Negative
fixtures retaining a forbidden call are explicit manifest exceptions and must
refuse without publishing an executable. No shallow/reset-only implementation
may be run with automatic drops while its owned payload contract is unproved.

Automatic drops and the existing manual release calls must never release the
same storage. Until G5 deletes the manual calls, the pass classifies every
release primitive it meets. The class comes from the runtime-call ABI owner,
not from matching a name:

- `reset(x)`: the call releases x's storage and leaves x as a valid empty
  value. The pass keeps x live and drops it at its last use as usual;
  dropping an empty value frees nothing. The current manual primitives are
  all this class:
  - `ArrayDrop` and `CompilerRetireArrayStorage` emit `pgy_array_drop_<T>`
    (C) or `pgy_array_drop_storage_raw_export` (LLVM). Each frees the backing
    and sets `data = NULL` and `length = capacity = 0`
    (`src/runtime/pgy_runtime_memory_array_slot_inline.h:268-279`).
  - `ArrayDropOwnedStrings` emits `pgy_array_drop_owned_String`, which also
    frees each element.
- `consume(x)`: the call releases x's whole footprint and x is never used
  again. The pass treats the call as x's last use and emits no automatic drop
  for x.
- Anything else is a refusal. The pass never guesses.

Explicit `Rc`, `Box`, `TextBuilder`, region, and Slot releases are not
ordinary values in this contract. They keep their own release edges, and the
pass treats such a handle as affine.

A `reset` call that frees only the backing of a collection whose elements own
heap storage leaks those elements; it cannot double-free them. G5 deletes the
calls and their classes, and a negative gate refuses their return.

### 5.6 Refusals

Every refusal is a typed diagnostic with a stable code. None of them lets
compilation continue.

| Code | When | Reported to |
|---|---|---|
| `OWN_MISSING_ADMISSION_FACT` | a callee identity, arity, local type, or type class is missing from admitted MIR | compiler (internal error naming the fact) |
| `OWN_MISSING_CALLEE_SUMMARY` | a call to a routine with no resolved summary, including an `extern` whose ABI declaration has no summary for a heap-typed parameter; never read as all-borrowed | compiler |
| `OWN_SUMMARY_ARITY_MISMATCH` | a summary whose length differs from the call's argument count or the routine's parameter count | compiler |
| `OWN_FUNCTION_VALUE_SUMMARY_MISMATCH` | a routine with a sink parameter converted into a function value, whose type is all-borrowed | user: names the parameter and the use that made it sink |
| `OWN_MISSING_STRING_CLASS` | a runtime call returns a `String` without a declared storage class | compiler |
| `OWN_UNCLASSIFIED_RELEASE` | a release primitive without a 5.5 class | compiler |
| `OWN_LOOP_CERTIFICATE_REJECTED` | the computed loop live set fails the check | compiler (a liveness bug) |
| `OWN_DUPLICATE_SINK_ARGUMENT` | after normalization one variable still reaches two sink parameters | compiler (the normalizer must bind a temporary) |
| `OWN_EXPLICIT_COPY_REQUIRED` | D1 (B): a collection must be copied because it is used again later | user: names the later use and, at a call, the sink parameter that stores the value; fix with `Clone(x)` or by reordering |
| `OWN_AFFINE_COPY` | a copy of an affine handle | user |
| `OWN_WORKER_BOUNDARY_SHARE` | a value crosses an `async`, `parallel`, or worker boundary without a move or an explicit copy | user (AGENTS.md worker rule) |

### 5.7 Observer

`--observe-ownership` prints JSON rows for each routine:

- its parameter modes, each with the reason that made it sink (stores,
  mutates, redefines, returns, passes to a sink parameter);
- each operand's decision (move, copy, lend), with its reason (dead after,
  used again at a location, borrowed);
- each drop and end, with the statement or edge it follows and the glue
  descriptor it uses;
- each copy, with its glue and, under D1 (B), whether it was implicit or an
  explicit `Clone`.

This meets the AGENTS.md rule that derived choices stay inspectable. Gates
read copy and drop counts from this output, never from program behavior.

### 5.8 Acceptance gates

Every fixture runs on both C and LLVM. Its stdout must equal the current
no-free build, and ASan/LSan must report no leak and no use after free. The
inputs are GPT's baseline programs (`.tmp/ownership-cleanup/code-baseline/`)
plus the model witnesses.

These are I8 acceptance requirements, not a claim that every production
fixture already runs. A syntax witness does not replace an execution check.

| Fixture | Model witness | Copies / evidence boundary |
|---|---|---|
| GUI inline | `gui_program_moves_only` | 0 |
| GUI through constructor and inout append | `gui_calls_sink_moves_only` | 0 |
| The same, reading the text after the constructor | `gui_calls_sink_reuse_copies_once` | 1, at the call site |
| Owned update `Append(xs, y)` returning `xs` | `owned_update_needs_sink` | 0 |
| `AddDraw(inout state.draws, label)` on a `GuiState` record | `gui_state_focus_copies_nothing` | 0, and 0 field copies |
| `state.draws = Append(state.draws, label)` | `gui_state_update_copies_nothing` | 0 |
| A read-only view of `state.draws` | `projection_view_copies_nothing` | 0, and 0 field copies |
| Copy of a live / dead / borrowed source | `elab_copy_*` | as in the decision table |
| Branch with an arm-only use; loop with a body-only definition | per-arm settle, loop certificate | 0, with the drops on the edges |
| Wide-live, 64/128/256 values | — | 0; doubling the input at most about doubles elaboration time |
| `let items = h.items` read with `h.name`, then `h` used whole | `overlapping_projection_copies_nothing` | 0, and 0 field copies |
| A part taken out of a record that is dead afterwards | `dead_aggregate_part_moves` | 0 |
| A string-concat temporary observed twice in its region; one escaping it | `region_value_released_once`, `region_escape_copies` | 0; 1 at the escape |
| An error after building two parts, before the pack | `error_releases_partial_parts` | both parts dropped on the error edge |
| An early return; a break out of a loop with a loop-local value | `early_return_elaborates`, `break_elaborates` | these witnesses only locate drop syntax; general execution uses `xelab_sound`/`routine_exit_is_uniform`, and I8 must execute the production exit paths |
| A two-function call chain | `chain_needs_two_rounds` | 0 after the fixpoint |

Negative fixtures, refused with the named code:

- a collection used again after being copied, without `Clone`
  (`OWN_EXPLICIT_COPY_REQUIRED`);
- a copy of an affine handle (`OWN_AFFINE_COPY`);
- a call to a routine whose summary is missing, and one whose summary has
  the wrong length (`OWN_MISSING_CALLEE_SUMMARY`,
  `OWN_SUMMARY_ARITY_MISMATCH`; witnesses `missing_summary_is_refused`,
  `arity_mismatch_is_refused`).

One safety shape changes sides; the retired manual builtin does not become
an accepted public cleanup API.
`tests/concept_semantics/hashmap/inout_string_array_deep_drop.pgy` pushes the
literal `"borrowed"` into an array and then deep-drops the array through an
inout parameter. The current analyzer refuses it, because the deep drop would
free a static string. Under 5.4 the push moves static storage into owned
storage, so it inserts a heap copy. The positive successor removes the manual
release and exercises compiler-synthesized cleanup after the inout call; it
runs without leaks or use after free. Keep a separate manifest-listed negative
fixture containing the retired call, with the stable refusal/no-artifact
oracle. Migrate the safety shape and the old rejection expectation together in
`tests/self_hosted/parity/collection_ownership_semantic_owner.sh:269-309`,
which expects the old rejection today. Do not merely flip that old manual-call
program to acceptance or delete its static-storage safety coverage.

### 5.9 Refinement obligations

An implementation claims to implement this model only when:

1. Its liveness, loop live sets, and mode table are certificates that the
   pass itself checks, as `elab` checks the loop head. A liveness bug then
   becomes a refusal, not a wrong drop.
2. Its decision for each MIR statement equals `elab`'s decision for the core
   statement that the statement maps to (section 5.3).
3. Its glue meets 5.4.
4. It places branch, loop, and routine-entry drops on the edges the model
   uses: per-arm settle, loop entry and exit, and routine entry.
5. Each exit (return, break, continue, error) releases, on its edge, the
   values outside its target live set, as `xelab` does (section 2.3,
   step 11).
6. Every target rewrite (`normalize_cleanup` administrative Skip removal,
   drop motion) requires `cleanup_equiv` (§2.6). Ordered source call/place/
   expression normalization is a different transformation: it must preserve
   source evaluation/trace, returned values and all continuing-exit inout
   recovery obligations (§5.10). Target equivalence alone is not its proof.
   `drops_commute` proves exchange of two adjacent distinct-variable drops;
   arbitrary release-set permutations and borrow-end motion need a separate
   derivation.
7. A focus moves only the part's storage, by a bitwise move out of and back
   into its slot. The rest of the root is not read, written, moved, or
   dropped while the focus body runs (`TE_Focus`, `pack_child_segment`). An
   unpack moves each part's descriptor out and frees only the record's own
   node (`TE_Unpack`, `unpack_after_pack`).
8. Its summary table distinguishes an unresolved summary from a resolved
   all-borrowed one, refuses every call and routine whose summary is missing
   or of the wrong length, and records which round of the fixpoint produced
   it (section 2.3, steps 6 and 7).
9. No drop is emitted for a type whose copies can still share a backing
   (section 5.4, order).

### 5.10 Production refinement closure (OPEN)

Added during the user-authorized cutover review, 2026-10-09. This section fixes
implementation obligations, not new checked theorems. The single-inout core,
read-only substitution and abstract heap soundness above remain bounded by
their actual definitions. No native/self-host/backend closure follows from
editing this contract.

1. **Admission before MIR.** Preserve admitted type/callee/arity/place facts
   and actual exclusivity checks. Replace variable-only inout and named-only
   readonly temporary policy **for ordinary values** with one-evaluation
   rooted-place and temporary facts. Preserve affine/authority named-boundary
   guards in the same native/self-host owners. Reject duplicate inout places, a root together with
   its focused field, and unproved dynamic-index overlap. Source `own` is not
   a substitute for inferred ownership. An AST/formal-mode-only guard cannot
   remain an earlier veto of normal programs the new MIR contract admits.
2. **Ordered call normalization.** Enumerate all inout actuals and the
   ordinary return value. Recover every inout obligation exactly once on every
   continuing normal, early-return and handled-error exit. Check/prove a
   normalization to the importing core or add the needed importing supplement
   before admitting it; do not label multiple outputs proved by `SCallIO`.
   Normalize nested calls, constructors, short-circuit arms and indexes with
   the existing typed graph and stable syntax identities. Evaluate each actual
   once in the language's admitted order; a skipped arm creates no owned
   temporary. An error cleans only parts already constructed. Do not rescan
   program roots or let recursive backend expression emission choose these
   facts independently.
3. **Result origin and view frame.** The target join owner is now named in
   §5.10.1; P1/P3 must bind the actual producer of result freshness, transfer
   and argument/part provenance. Existing
   Slot rows and the limited Allocator/TextBuilder join do not supply it.
   Existing Slice/indexed-String facts provide starting backing/place inputs,
   not a proved current-lifetime/generation issuer. Resolve that issuer and
   loan/refinement or refuse the affected automatic-elaboration boundary.
   Extend the
   backing owner's liveness through the view's actual last use. End a view
   without releasing its backing; a mutable Slice stays write-through.
   Reallocation/reset/escape and worker boundaries require their existing
   invalidation or transfer checks. Test live-view push/grow/reset and
   inout/sink transfer, not only later reads. Extending backing liveness can
   change a formerly dead direct source into a live one: any actual independent
   copy must obey D1 and report the view dependency in its observer/diagnostic.
   Do not use a silent copy/Clone to change write-through view semantics or
   erase a loan conflict. A descriptor, heap pointer, source ID or storage
   domain is not a current ownership certificate.
4. **Mutation and allocator.** Test replacement, discard, removal, clear,
   duplicate insertion, relocation, nested clone and partial-failure cleanup
   with the same descriptors/operation postconditions on both backends.
   Preserve the allocator attached to the backing; do not `free` a region or
   custom-allocator pointer. A recoverable/continuing failure leaves the old
   admitted owner valid and consumes only the declared parts. A terminal
   panic/abort has a separate expected-exit/sanitizer policy and is not an
   `XThrow` or proof of restored ownership. Ordinary success/handled-error
   fixtures keep ASan/LSan enabled with zero leaks/UAF; abort fixtures are
   reported separately and cannot replace that positive evidence.
5. **One generation of evidence.** Refresh and check liveness after final
   expression/place rewrites and DCE. Bind summaries, glue, view dependencies,
   instruction/operand facts and JSON projection to that admitted snapshot.
   Unknown, stale and cross-snapshot facts refuse. Canonical JSON parity does
   not erase identities or equate independently minted raw IDs.

These obligations are exercised by the cutover plan's F1 acceptance/falsifier
matrix. Required focused gates block dependent implementation. Negative
manual-release fixtures are manifest exceptions, not admitted programs; they
must refuse and produce no executable. Neither a reduced test count nor a
structural registration check replaces actual C/LLVM sanitizer, full
bootstrap, measured 3 GiB pressure or exact-SHA CI evidence. P1 must also
decide retired-builtin name tombstones and bind each negative fixture's
exact stable code to the existing diagnostic owner. Deleting operational
builtin rows does not automatically authorize name reuse or settle that code.

### 5.10.1 Coupled call, recovery and view contract (design fixed; proof OPEN)

Adopted in the 2026-10-09 prerequisite review. These are compiler-internal
contract decisions, not additional source annotations, an implementation or
new theorems. Choose the call schema with recovery outputs and view origins,
not a separate ABI vocabulary that later loses those facts.

**Owner and identity.** The ownership-clean owner produces one admitted call
contract from resolved call identity, routine summaries, type/glue facts,
value origins and lifetime dependencies. It joins rather than overrides the
existing owners: `abi.runtime_call_rows` for admitted runtime operation
identity/physical selection, `abi.layout_rows` for representation, and the
semantic value-origin owners for ownership. A source routine is keyed by its
resolved callable identity; a runtime operation by its existing admitted
`RuntimeCallAbiId`. Source spelling and target symbol names are not identities.
The limited Allocator/TextBuilder join is not expanded by assertion into a
general collection/String owner.

The joined fact is `OwnershipCleanCallContract`, indexed by admitted snapshot,
routine, typed call site and ordered operand identities. It is produced once
by the ownership-clean pass and carried to normalization and emitters; those
consumers do not separately decide ownership from ABI shape. This names the
target join owner; no production producer exists yet. Before its P3/P4 source
implementation, declare its owner/carrier and complete consumer inventory in
the SoT registry. Do not mark an existing ABI row CLOSED for these new columns.

| Contract field | Required meaning |
|---|---|
| Inputs | ordered formal/actual identities, types, admitted borrow/sink/inout modes, and rooted places for inout |
| Recovery outputs | one entry per inout formal, in formal order; exact output index, type, backing responsibility and continuing-exit recovery |
| Ordinary outcome | normal/early-return value or handled-error payload, separate from all recovery outputs; no ordinary return consumes a recovery output |
| Output origin | scalar/static, fresh with allocator/glue identity, transfer from an exact argument/part, backing view, or payload view |
| View dependency | backing owner/definition, place/range, access permission, static focus lifetime and snapshot; resource/graph leases retain their own issuer; absence is not a fresh allocation |
| Mutation postcondition | payload read/write versus possible backing relocation/reset/retirement/transfer; replaced or discarded payload responsibilities |
| Evidence binding | one final MIR generation for the call summary, places, outputs, origins, glue and lifetime facts; missing/stale facts refuse |

A synchronous argument-retention row grants only call-duration borrowing;
it cannot establish a fresh result or a returned view's lifetime. C/LLVM and
self-host consumer migration belongs to the later transition after P1, not
this contract-only review.

### 5.10.2 Multi-inout bundle normalization

For disjoint admitted actuals `g(inout a, inout b, ys) -> r`, use the existing
core operations as the proof representation:

```text
SPack t [a, b]
SCallIO g_adapter t ys
SUnpack t [a, b, outcome]
decode outcome into the ordinary return/error continuation
```

The callee adapter receives the inout bundle, executes the original body,
and uses one recovery epilogue to return every current inout value plus an
ordinary outcome packet. An internal early return is caught by that epilogue;
it is a normal function return to the caller, not an early return from the
caller. A recoverable error is carried with the recovery values. **Unpack
precedes error propagation/handler selection.** A `throw` escaping `SCallIO`
before unpack is not covered by the current core or exits layer and is not
an admitted implementation of this adapter. Terminal panic/abort remains the
separate boundary in §5.10(4).

Before packing, admission rejects repeated or overlapping places, a root
together with its descendant, and dynamic overlap without a disjointness
certificate. Different variable IDs do not prove different backing. Core
`SPack` already checks `nodupb` for exact repeated variables; it refuses rather
than silently copying them. The normalizer is the first alias defense before
the bundle can hide actual identities. Source-place admission is still needed
before normalization. Borrowed actuals cannot alias the transferred inout footprint.
The private bundle and outcome names are compiler-issued fresh identities.

Actuals, nested calls and index expressions are evaluated once in admitted
source order. Failure during actual evaluation cleans only constructed
temporaries and leaves not-yet-transferred caller responsibilities intact.
The output schema fixes recovery count/order/types on every continuing exit;
failure is not permission to omit a field or reconstruct it from an old copy.

P1 must define the unnormalized multi-inout source-call semantics and prove
normalization's trace/value/ownership preservation, then reuse the importing
cleanup theorems. The existing propositions execute the normalized caller
and adapter; they are not equivalence to an unnormalized call semantics. P1
does not add a second heap or infer that the one-output core theorem already
proves the source transformation. The production pointer ABI may pass the
original pointers without a heap tuple. That requires a separate refinement:
the private-bundle representation is unobservable, exclusivity is retained,
and no hidden tuple allocation/copy is introduced. `TE_Pack` itself has an
abstract allocation, so a zero-copy model probe alone is not that cost proof.

### 5.10.3 Write-through views and static focus lifetime

`Slice` remains a non-owning write-through view. It is not the read-only alias
substitution in `OwnershipCleanReadOnly.v`. The selected discipline for
compiler-local writable views is the whole-backing focus checked in
`OwnershipCleanViewScope.v`, for its bounded, nonescaping/no-call scopes:
the backing is suspended until the view's last use and its exact storage is
restored afterwards. No extra cleanup ledger or public Slot per Array is
needed for that model. This does not yet issue lifetime facts for general
Slice uses. Resource/graph boundaries retain their authorized BorrowLease/PinLease
issuer from `OwnershipTeardownAuthority.v`; those leases are not the static
compiler-view issuer. An AST ID or MIR snapshot number must not be cast into
a live graph handle or release authority.

The required production compiler fact contains the owner definition, rooted
backing place/range, view identity and permitted access, under the final MIR
snapshot. Its issuer
derives those facts from the admitted Slice construction and checked owner
liveness. The backing's live set includes every transitively derived view,
view alias and admitted returned-view dependency. A static view scope ends only when no
reachable view use remains on that path, including loop/branch/call edges.
Unknown origin or absent lifetime evidence blocks automatic elaboration.
Current native lexical Slice facts are starting inputs, not this final issuer.
The static issuer covering these derived/aliased/returned dependencies is
OPEN; `view_admitted` only checks an already chosen core scope.

While a writable view's backing focus is active:

- in-range element reads and permitted write-through updates through the view
  can be admitted; naming the suspended backing itself is refused by CL7;
- backing growth/reallocation/reset/drop or a consuming/inout transfer that
  can perform those actions is refused;
- a payload replacement must also respect any live borrowed payload, allocator
  and copy/drop-glue obligations; protecting Array storage alone does not
  protect a borrowed old String element;
- view writes, range/exclusivity and worker/resource boundaries have explicit
  checks in addition to lifetime. Lifetime protection alone does not grant writes;
- view end releases no backing; a silent independent copy cannot cure a loan
  conflict or change write-through semantics.

Keep the runtime descriptor `{data, length}`. The default adds no runtime
generation or layout field. A changed view ABI requires a separate decision.
P1 must prove issuance/currentness, the source-to-core view frame and guarded
structural operations. Production liveness integration and C/LLVM consumers
remain later implementation obligations, not CLOSED by this section.

### 5.10.4 Bounded importing evidence (2026-10-09)

`OwnershipCleanCallRecovery.v` imports the existing value and exit machines.
`normalize_multi_call` refuses repeated output identities, inout/ordinary-
argument aliasing and private-name collisions **before** packing. Its caller-
scope input must contain every already-issued binding; it checks operand
membership and rejects a bundle/packet name present anywhere in that scope,
not just among call operands. Completeness and final-snapshot binding of this
input remain obligations of the production typed-scope owner. Without
that check, a fresh bundle name would hide the original inout/readonly alias
from the core's check and cause a silent preservation copy. The admitted
pack lemma, direct first-pack elaboration copy bound and independent alias/
existing-local collision falsifiers prevent that path in the importing scope.
The copy bound also requires every inout to be absent from `B` and uses
exactly the admitted `bundle :: args` live set, not an arbitrary caller
continuation's live set; it is not a
whole-callee copy bound or a production allocation/cost theorem.
`multi_call_pack_call_unpack` proves ordered source execution once the actual
normal-only adapter body runs and satisfies its result-shape premises.

`RecoveryAdapter` and `recovery_exec` define one operational catch boundary
over the existing target heap, not ordinary exit-skipping `XTSeq`.
`compile_recovery_adapter` compiles its normal/error epilogues together and
rejects unbound break/continue in the actual body. The soundness theorem
`one_compiled_adapter_recovers_all_continuing_outcomes` preserves INV/CORR and
packages every recovery output and independent outcome payload. Borrowed
parameters are retained under the existing frame discipline; outputs/private
names cannot be borrowed. The lower-level `post_exit_packages_every_output`
is only a post-exit packaging lemma, not the adapter's execution rule.

`decode_recovery_value` checks output arity, packet shape and success/error
tag. `dispatch_recovery` restores ordered inout bindings before a value-level
handler observes them. Wrong shape/tag/repeated outputs return `None` rather
than relying on stuck unpack. General language type-schema checking and the
lowering of this decoder/dispatch into caller code remain OPEN. The audit
executes a two-inout/result core call with a readonly argument without leaks,
and one compiled branch adapter handles both return and error inputs.

These are real importing propositions. Section 5.10.5 supplies an alternative
status-lowered callee in `SProcTable`/normal `SCallIO`; it does not refine
`recovery_exec` itself into that call rule. Caller decoder/handler code,
actual source expression/index ordering, physical place disjointness and
erasing the abstract bundle without hidden tuple allocation remain OPEN. No existing
callee-summary premise is asserted true merely by writing this supplement.

`OwnershipCleanViews.v` is a **dynamic ghost oracle for the intended static
view evidence**, not a proved static liveness issuer. Its checks read the
model's current environment/heap and non-reused abstract block IDs.
`view_kind`/`LeaseKind` is shared metadata, not an admission discriminator
or a proof of write/exclusivity permission. Scalar
write-through uses the **same** owned heap. A successful write preserves
the core INV/footprint, updates the source backing and preserves CORR, and
reads back through that backing. A concrete guarded
backing drop refuses active views and refines the existing `TE_Drop` after
admission. View end changes
only evidence; `ended_ticket_never_usable_again` assumes `views_wf` before
the ending operation and covers subsequent `ViewSchedule` steps, including
both issuance and end. The claim is not limited to issuance-only schedules.
This does not permit replaying an older evidence-state
copy. The actual single-current-state/snapshot issuer is still OPEN.
An active view blocks structural operations whose admitted effect
fact lists its owner. The theorem does not prove that a production effect
set lists every affected owner; that set's actual owner must still admit it.
General owning-payload replacement/drop glue, full final-MIR static liveness,
linear evidence/snapshot binding, mutable exclusivity and graph/backing
binding remain OPEN. Except for the dedicated guarded drop consumer, the
base `texec`/`elab` operations still know no views: protecting every move,
pack, call, focus, unpack, growth and cleanup settle needs the later whole-
instruction frame/refinement. A move out and back is not made safe by a
subsequent footprint-equality check. Scalar-only
updates do not establish the String-replacement rule by analogy.

This evidence narrows the P1 obligations; it does not close P1, widen a SoT
row, implement a compiler issuer or replace the eight full-cutover gates.

### 5.10.5 Exit lowering and view scope evidence (Claude, 2026-10-09)

Two importing supplements have recorded kernel checks with no assumptions:
`OwnershipCleanCallLowering.v` and `OwnershipCleanViewScope.v`. The first
connects an admitted exiting callee to the normalized caller through the
ordinary call rule. The second proves a bounded alternative view design;
it does not close CL7's requested Views-oracle refinement or full static
issuer. Neither is an implementation.

**Exiting callee in the ordinary call table.** `lower` turns an `XStmt`
callee body into an exit-free `SStmt`. A status variable records normal,
break, continue, return or error, and every later statement is guarded by
it. A loop consumes its own break and continue. The adapter unpacks the
inout bundle, runs the lowered body and repacks every inout formal with the
packet `[tag; value]`. It is an ordinary `SProcTable` entry, so the caller's
`SPack`→`SCallIO`→`SUnpack` runs through the core's own call rule and its
target soundness is `elab_sound`. No catch rule is needed on this path.
`recovery_exec` remains an alternative model, not a premise.

- `lower_sim`: every source run of the body is simulated by its lowering,
  with the same trace and final values outside the issued names, and the
  outcome is recorded as a status code.
- `adapter_exec` and `lowered_multi_call_recovers`: given the body execution,
  defined output/value and admission premises, on every continuing exit
  the caller gets back every inout and the packet. Caller handler dispatch
  is a subsequent obligation, not an execution proved by these theorems.
  `lowered_multi_call_sound` gives the same in the ownership machine.
- The witness callee has a local loop that breaks and a return/error
  switch. It elaborates, and the whole caller frees every block on both the
  return run and the error run.

Two consequences need a decision before P7.

1. Liveness does not see that status-guarded paths exclude each other. The
   adapter therefore initializes the ordinary value and every listed body
   local to a unit leaf, and every lowered loop head keeps the outputs and
   locals. No general theorem proves refusal for an incomplete local list;
   successful elaboration still has the core's conditional safety guarantee.
   The same effect lengthens lifetimes. A production epilogue
   that jumps straight to the repack, in the style of `xelab`, keeps
   lifetimes exact, but that refinement is not proved here. Under the 3 GiB
   cap, prefer the direct jump and treat the flag form as the proof route.
2. `sgx value = Some v` and the inout output lookups are theorem premises,
   not obligations checked by `adapter_admitted`. A production issuer must
   establish them on every continuing exit, including unit-returning routines,
   and establish completeness/currentness of the local list.

**Writable view as a whole-backing focus.** A writable view `v` over a
backing `y` is `SFocus v y [] body`, scoped from the view's creation to its
last use. During the scope the backing is suspended. Source semantics hands
its value to `v`. The target rule moves `y`'s exact blocks to `v` and
removes `y` from the owned environment. Elaboration refuses any live use of
`y` inside the body. At the end the view's value moves back into `y` with no
copy.

- `view_admitted` refuses any mention of `y` in the scope. It also refuses
  any use of `v` other than element read, element write through a one-index
  focus, and observation. The view cannot grow, be copied, escape, be
  passed to a call or be unpacked.
- `view_scope_keeps_backing_shape`: after the scope the source backing has
  the same length. It does not state a physical data-pointer invariant or
  descriptor validity at every intermediate point. `view_scope_sound`
  composes the postcondition with `elab_sound`; the production descriptor/
  pointer refinement is still required.
- An element write redefines the focused element, so the core's settle
  releases the old element. This is the payload-replacement rule for this
  path.
- The witness writes through the view, pushes onto the backing after the
  scope, and frees every block. The core alone refuses growth, copy-out and
  growth between two view uses inside the scope.

This is a boolean admission check for a manually chosen core scope, not
the full static lifetime issuer required by §5.10.3. No theorem connects it
to `OwnershipCleanViews.v`'s dynamic oracle. It uses no ticket ledger,
runtime generation field or lease vocabulary. Resource and graph boundaries
keep their own lease issuer. The
developer-visible cost: while a writable view is live, the backing itself
cannot be named in the scope, either for reads or for writes. A 2026-10-09
lexer/AST census of all 2531 tracked self-host sources found 76 Slice member
constructions, all immediate call arguments, no local constructions and no
Slice return declarations. All 32 Slice formals use the default mode, not
inout. This source census is not a transitive call-effect or alias proof.
All 76 call-argument constructions are outside this no-call model's coverage;
this is zero coverage of that census, not proof the calls are unsafe.
It establishes that these existing call-bounded usages cannot simply be
rejected by applying the CL7 no-call writable rule to every Slice. P1 must
bind their read-only call summaries and backing lifetime separately; writable
views keep the focus rule. Details and exact boundary:
`../audits/ownership_slice_dx_census_2026-10-09.md`.

Still OPEN: actual caller packet decoding/error dispatch and propagation;
unnormalized multi-inout source semantics and normalization equivalence;
complete local/output issuance; production direct-jump epilogue refinement;
place disjointness beyond variable identity; source expression order above
`SStmt`; the Views-oracle connection and static lifetime issuer for derived,
aliased, returned or stored views; views passed to calls; several
writable views of one backing; sub-range index bounds; and production
descriptor and pointer refinement.

### 5.10.6 Direct-control refinement boundary (GPT, 2026-10-09)

`OwnershipCleanDirectControl.v` implements a finite instruction lookup over
structural source-tree labels. Its only instructions are an existing `SStmt`,
a jump or a condition branch. Normal/return/error continuations are labels;
loops consume their own break/continue and a try body jumps to its handler
on error. This construction introduces no status/guard bindings in `SEnv`
and does not initialize body locals. Missing structural paths return `None`,
not fallthrough. Graph labels are not user variables or resource handles.

`direct_control_simulates_source` proves forward preservation of source
trace, final environment and exact exit destination. The importing consumer
executes a return/error switch with a skipped suffix and checks local loop
and handler routing. `direct_control_has_core_reference` connects the same
source execution to CL6's ordinary-core lowering, with the same observable
trace and environment outside its issued names. It does not replace CL6's
inout recovery theorem or introduce `recovery_exec` on that call path.

This is a source-control refinement slice, not full epilogue closure. Reverse
graph adequacy, effectful expression normalization, physical pointer/bundle
erasure, final-generation recovery live sets and actual MIR/C/LLVM graph
correspondence remain OPEN. In particular a forward witness cannot certify
every run of an emitter, and a graph without status variables is not a
measured whole-compiler memory improvement. No production drop is activated
by these propositions.

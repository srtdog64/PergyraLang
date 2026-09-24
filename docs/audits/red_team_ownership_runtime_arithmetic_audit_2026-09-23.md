# Red-Team Audit: Ownership, Runtime Memory and Arithmetic Legs

Status: READ-ONLY FINDING — REPRODUCED, NOT REPAIRED, except the items listed
under "Repaired in this round" and "Second round".

Observed tree: `81dbd5d2`, content-equal to main `70531de2` on 2026-09-23. A
frozen WSL build of that tree ran every reproduction on four legs: native C,
native LLVM, default (self-hosted) C and default LLVM. C legs were rebuilt from
emitted C with `-fsanitize=address,undefined` where ASan is cited.

Four red-team passes ran: type and ownership soundness, runtime memory safety,
arithmetic and conversion semantics, and intent/authority bypass. The
intent/authority pass completed, but its report was withheld by a safety
filter, so it contributes no finding here. The M1 reproduction below was re-run
independently of the red team and matched exactly, including the ASan frames.

Several defects sit in files that the main checkout is changing without a
commit yet (`pgy_runtime_panic_contract.h`, `option_result_runtime_owner.pgy`,
`runtime_call_rewrite_owner.pgy`). An earlier version of this note also named
the map runtime headers; that was wrong, and the second round below repaired
them.

## Why these reached main

- `tests/compare_backends.sh` sets `PGY_NATIVE_PIPELINE=1`, so it compares
  native C with native LLVM only. No general harness compares the default route
  with native C, so a wrong answer on the default route is caught only when a
  focused owner gate happens to run that exact shape. This round adds one such
  gate for the shapes it repaired; it is not a general harness.
- `text-builder-owner-test-smoke` has no CI caller. The default C route accepts
  `tests/cases/text_builder_owner/copy.pgy`, which the gate says must be
  refused, and the binary then double-frees.

## Memory safety: accepted programs that corrupt memory

**M1. A String borrowed from a collection outlives a mutation of it. All four
legs accept it.**

```pergyra
let m: HashMap<String, String> = MapNew();
MapSet(m, "k", Concat("value-", "abcdefghijk"));
let s: String = MapGet(m, "k");
MapRemove(m, "k");
MapSet(m, "j", Concat("other-", "zzzzzzzzzzz"));
Log(s);   // prints other-zzzzzzzzzzz, rc 0
```

ASan reports a heap-use-after-free: the value is freed in
`pgy_map_remove_string` (`pgy_runtime_map_string_inline.h:388`) and read in
`pgy_log_string`. The same shape fails through a `MapSet` overwrite (`:239`),
`ListGet` followed by `ListRemove` or `ListSet` on native, and
`ArrayDropOwnedStrings` followed by a read. `docs/semantics/04_ownership_abi.md`
says `MapGet` and `ListGet` return a pointer the collection owns, and neither
checker tracks the result as a borrow of it. Fix options: return an owned copy,
or refuse a collection mutation while such a result is live.

**M2. Rc and Box handles copy freely. Native accepts.** `let c: Rc<Int> = r;
RcDrop(r); RcGet(c)` reads freed memory, two `RcDrop` calls through copies
double-free, and a generic `Id<T>(r)` copies the same way. Native C's inline
`pgy_rc_drop` returns silently on a NULL control block, while the LLVM export
panics, so the two backends also disagree. `type_is_builtin_owner_handle`
(`type_checker_helpers_resources.c:79`) treats only TextBuilder as a
single-owner handle.

**M3. A shallow Array copy grows into freed storage. All legs accept it.**
`let w: Array<Int> = v;`, then 64 pushes to `v`, then `w[0]`, is a
heap-use-after-free after realloc. `Mix(v, v)` with
`func Mix(inout a: Array<Int>, b: Array<Int>)` is the same defect through an
alias. The inout alias check (`type_checker_helpers_late.c:191-209`) compares
only inout pairs.

**M4. A Slice borrow survives storage invalidation.** The following all reach a
freed backing array on at least one leg:

- passing the array to an inout function that grows it;
- a Slice of a Slice;
- a Slice returned from a helper;
- a Slice of an inout parameter;
- rebinding the backing array.

`docs/semantics/04_ownership_abi.md:262-265` says reallocation and rebinding
are rejected while the view is live.

**M5.** The String from `TextBuilderFinish(tb, pool)` is read after
`AllocatorDestroy(pool)`: heap-use-after-free on every leg.

**M6.** A copied Allocator value destroyed twice gives a use-after-free and a
double free on the C routes. The default LLVM route refuses the same program.

**M7.** `CharCode`, `CharAtN`, `SubstringWithLen` and the `Sub*WithLen`
family trust a length the caller passes (`pgy_runtime_string_window_inline.h:22-85`,
`pgy_runtime_strview_inline.h:65-82`). A longer length reads past the heap
allocation. These are public builtins.

**M8.** The default C route has no missing-return check for a non-`Never`
function, so `func Pick(x: Int) -> Int { if x > 0 { return 1; } }` falls off
the end, which is undefined behavior. Native refuses it.

**M9.** The default C route accepts a TextBuilder copy, and the binary
double-frees. Native refuses.

**M10.** On the default C route `UnwrapOption(None)` and `Unwrap(Err)` return
the payload slot instead of panicking. The cause is
`#define pgy_option_unwrap(o) ((o).value)` from
`option_result_runtime_owner.pgy:411`. The default LLVM route aborts without a
panic line.

**M11.** The default C route accepts slot flows that native refuses: copy, then
`Release`, then `Read`; and `Consume(own s)`, then `Read(s)`.

**M12.** Pushing to an array inside `foreach` over the same array is a
heap-use-after-free on the default C route.

## Runtime robustness

**R1.** Map growth counts tombstones: `(count + deleted + 1) * 4 > cap * 3`
doubles capacity regardless of live entries. Two million set/remove pairs
leave zero live entries in a table of capacity 1,048,576. The sites are
`builtin_hashmap_inline.h:274/413/509/605`, `builtin_storage_inline.h:317`,
`map_int_key_inline.h:38`, `raw_map_exports.h:309/532`,
`raw_map_key_exports.h:35/120/256/446/566/686` and
`map_string_inline.h:509/669/777`.

**R2.** When growth fails, `MapSet` warns on stderr and drops the write. That is
a silent fallback, and the next `MapGet` panics with "map key not found".

**R3.** The panic paths call `abort()` without flushing stdout, so the last
output before a panic is lost when stdout is a pipe or a file.

**R4.** `ToInt` of an out-of-range value differs by platform and backend: an
int32 cast of `strtol` on the C runtime, and `atoll` (undefined behavior) on the
default C route. `"abc"` becomes 0 with no error.

**R5.** The default LLVM emitter places `MapSet`'s value temporary `alloca` in
the loop body, so a 200,000-iteration loop overflows the stack.

## Arithmetic and conversion

**A1.** The default C route carries Float as a C `double`, and `ToString` of a
Float truncates to an integer: `float_to_string_precision` prints `3|0|100` on
the default C route. The main checkout has uncommitted work that adds
`pgy_tostr_float` and `pgy_tostr_double`.

**A2.** No single owner decides the range of an unsuffixed integer literal, so
the legs disagree on `-2147483648 * 2`, `2147483648` and `4294967296 / 65536`.
The default C route accepts `99999999999999999999999`.

**A3.** Float `%` passes both type checkers, and then no backend can build it.

**A4.** The type rules diverge. Assigning an Int to a Long binding is accepted
by native and refused by the self-host checker. `Int < Float` is accepted on
the default C route and refused by native.

## Found while checking an external review of `70531de2`

**E1. Method receiver order.** `Make("r").Pair(Tag("a"), Tag("b"))` printed
`a b r` on native C and on the default C route and `r a b` on native LLVM.
One argument split the same way. Native C is repaired in this round (below).
The default C route still prints `a b r`; its emission owner
(`expr_semantic_call_emit_owner.pgy`) has uncommitted work in the main
checkout. An index-expression receiver and dynamic ability (vtable) calls are
not on the ordered path either.

**E2. Runtime attribute tables reach user declarations (latent).**
`llvm_run_optimization` (`llvm_api.c:338-372`) applies the runtime name tables
to every declaration in the user module, not only runtime entrypoints. A name
containing `panic` gets `noreturn` and `cold` by substring
(`llvm_runtime_attrs.c:15`), and the readnone/readonly tables match by exact
name. User definitions are exempt, but a user `extern "c"` declaration is a
declaration too. Today no source reaches it: the driver links no user object,
and every runtime or libc symbol that matches really never returns. It becomes
reachable once user objects can be linked, so the table should be keyed on
"this symbol is a runtime entrypoint" rather than on spelling.

**E3. A red gate that CI never runs.**
`tests/self_hosted/parity/one_mir_string_case_math_projection.sh` fails at HEAD
("current producer rejected source"), and it failed the same way before this
round. Gate reachability counts it as covered only because the component
contract names it in a `require_file` line; no Makefile target or CI step runs
it.

**E4. Diagnostic quality.** The default route refuses an out-of-range Long
literal before emission, but the message is
`PARSER GRAPH ERROR: import composition produced invalid expression graph rows`
instead of native's "Long literal is outside the signed 64-bit range".

## Repaired in this round

- Native C emitted a Float literal with `%g`, six significant digits. It now
  uses `%.17g`, and `float_literal_round_trip` in `tests/compare_backends.sh`
  compares it with native LLVM.
- The default C route's `Abs`, `Min` and `Max` helpers were `int32_t` and
  truncated a Long operand. They now keep the operand's type.
- The self-host front end accepted a Long literal past
  `9223372036854775807` and the C compiler wrapped it. It is refused now.
- `tests/self_hosted/parity/default_route_scalar_value_owner.sh` runs those
  shapes on the default route. It also runs the C reserved-word escape case,
  whose `backend_compare` case was native-only and could not see the self-host
  defect it was added for.
- Native C evaluates a call receiver before the method's arguments (E1), and
  `method_receiver_before_arguments` in `tests/compare_backends.sh` compares it
  with native LLVM.

## Second round, 2026-09-23 evening

Reproductions ran on WSL builds of main with a per-tree `TMPDIR` (see E5) on
all four legs.

### Repaired, with the gate that now fails if it returns

- **R2.** A failed map, set, list or queue operation panics: class `oom` for
  allocation and size-overflow failures, `internal-invariant` for a broken
  invariant. The hashmap runtime smokes run every injected failure point as a
  child process and require the panic; `runtime_panic_contract_smoke.sh`
  forbids the old warn helper under `src/runtime`.
- **R1.** A rebuild keeps the capacity when live entries fit in half of it.
  Two million set/remove pairs on `HashMap<Int, Int>` now stay at 2 MB RSS on
  native C, native LLVM and the default C route (12 MB before). The hashmap
  runtime smokes check capacity after 100,000 pairs.
- **R3.** The panic emitter flushes stdout first.
  `tests/self_hosted/parity/runtime_panic_leg_owner.sh` checks the output
  printed before an index and a division panic on all four legs.
- **M10, C half.** The default C route panics on `UnwrapOption(None)`,
  `Unwrap(Err)` and `UnwrapErr(Ok)`; the same gate checks class and reason.
- **E1.** The default C route evaluates a method receiver before its arguments,
  and both C routes now order index receivers, a field of a call result and
  party ability (vtable) calls. `default_route_method_receiver_order_owner.sh`
  compares native C, native LLVM and the default C route. What stays open is
  in docs/205 section 11.1.
- **E3.** The gate was red because the self-host semantic typed a call argument
  of `Min`, `Max`, `Abs` or `Clamp` as Unknown and refused `Max(lo, Min(hi, v))`
  and `Max(Twice(3), 4)` on both default routes. That is repaired; the gate is
  green and runs in the Linux push shard, and `default_route_scalar_value_owner.sh`
  runs the shapes on all four legs.

### Found in this round

- **E5. The runtime object cache was shared across checkouts (repaired).** The
  cache key named the compiler revision, toolchain, profile and target but not
  the source tree, and freshness only asked whether this tree's runtime sources
  were newer than the object. A second worktree that built last supplied the
  runtime for every other tree using the same temp directory. The key now
  hashes `PGY_RUNTIME_DIR`.
- **E6. `UnwrapErr` exists only in the self-host front end.** Native C reports
  "Undefined function 'UnwrapErr'", while the default C route compiles it. A
  program accepted on one front end and refused on the other; open.
- **R5, wider.** The default LLVM route also segfaults (rc 139) on the
  two-million-iteration `HashMap<Int, Int>` set/remove loop above.
- **M10, LLVM half.** On `UnwrapOption(None)` the default LLVM route aborts with
  no panic line and loses the output printed before it.
- **A2, two more shapes.** Native refuses `Max(3000000001, Twice(big))` with
  "cannot assign 'Long' to 'Int'"; the default C route refuses
  `Max(Twice(big), 3000000001)`, which native accepts.
- **Observation, not investigated.** A loop that binds `ToString(i)` 400,000
  times holds about 14 MB at exit on every leg, against 2 MB for the Int-keyed
  loop; the temporaries look unreleased.

### State of the collection-ownership lane

The main checkout's uncommitted collection-ownership work was copied into a
local snapshot branch and built separately, without touching the checkout.
With it, `make self-host-compiler` fails, and native C and native LLVM still
print the overwritten value for M1 (`MapGet` then `MapRemove`/`MapSet`) and a
freed buffer for the `ListGet`/`ListSet` shape. M1, M3, M4 and M12 stay with
that lane.

## Third round, 2026-09-24

A dogfooding session building an agent harness in Pergyra reported language
problems as PP-001 to PP-034 (its record lives outside this repository). The
ones this round repaired, and the audit items it closed, each with its gate:

- **M8, PP-017.** The self-host semantic refuses a non-Void function that can
  reach its end, by native's flow rules, with `PGY_SEM_MISSING_RETURN` in text
  and JSON (`missing_return_flow_owner.sh`). An empty uncalled body no longer
  reaches MIR lowering.
- **M2, M6, M9.** Rc, Weak, builtin Box, Allocator and TextBuilder copies are
  refused on both front ends; a second RcDrop panics on both native backends
  (`single_owner_handle_owner.sh`, `text-builder-owner-test-smoke`, now in CI).
- **A2, A3, A4, E4, R4 undefined behaviour.** One owner decides the Int and
  Long literal ranges, Float `%` is refused, Int widens into Long, and ToInt
  is `strtoll` narrowed the same way on every leg
  (`numeric_literal_conversion_owner.sh`).
- **R5, E2.** Loop temporaries stay in the entry block on the default LLVM
  route and on native LLVM, where a StringSplit loop and a parallel join in a
  loop segfaulted; runtime attributes go only to registered runtime
  entrypoints (`loop_stack_storage_owner.sh`,
  `llvm_runtime_entrypoint_attrs_smoke.sh`).
- **PP-007.** A parent-relative `-o ../gen/x.c` on the default route
  (`parent_relative_output_path_owner.sh`).
- **PP-020.** Args() hands UTF-8 on Windows (`process_args_utf8_smoke.sh`,
  Windows push shard).
- **PP-021.** More than one `extern "C"` block (`extern_block_identity_owner.sh`).
- **PP-025.** An extern member may declare caps and effects.
- **PP-030.** `return TextBuilderFinish(builder, ...)` is refused on both front
  ends, as native already did.

Still open, each needing a decision rather than a fix:

- **Capabilities are optional and do not propagate (PP-024).** A function with
  no `with caps` clause may call ReadFile, and a caller without caps may call a
  callable, or an extern, that declares them; both front ends accept this.
  Making undeclared effects fail closed would change most existing programs.
- **ToInt has no failure contract.** `ToInt("abc")` is 0 on every leg; about
  550 calls, many in the self-host compiler, depend on that.
- **M10 on the default LLVM route.** The fix exists but makes every record
  program reference the runtime panic export, which breaks about twenty
  direct-MIR gates that link IR without the runtime; the helpers should be
  emitted only when used.
- **Zone and subject lending (PP-022), FFI string ownership (PP-002), FFI
  headers and linking (PP-004, PP-005, PP-019), stdlib gaps (PP-003, PP-006),
  receiver-field collection mutation (PP-027).** Design work; PP-027 belongs
  with the collection-ownership lane.
- **M5, M7, M11.** A pool-lifetime fact, a length-carrying text view, and slot
  and move flow in the self-host checker, as recorded above.

### Parallel access

The native `parallel` checks decided by name: a collection binding could not
be captured, and at most one task could assign a binding. Six falsifiers
looked for storage reached without the name; two were accepted and raced
under ThreadSanitizer on native C, and both native backends lost updates.

- **DRF-1. A method call writes the receiver (repaired).** Two tasks calling
  `counter.Step(1)`, where `Step` assigns `self.n`, contain no assignment to
  `counter`, so both were admitted. The same held one call deeper, through a
  bare field name inside the method, and when each task handed `counter` to a
  default-mode parameter, which writes the caller's value.
- **DRF-2. An aggregate shares a collection's storage (repaired).**
  `Holder(arr)` copies the Array header, so a task reading `holder.data[5]`
  read the elements another task wrote through a split half of `arr`. Nesting
  (`outer.inner.data`) and `Option<Array<Int>>` behaved the same.

`src/semantic/parallel_capture_write_reach.c` and
`src/semantic/parallel_capture_storage_reach.c` answer them. A task writes
through a binding when it assigns a path rooted at it, calls a method whose
body writes `self` (followed to depth 8), or hands the binding anywhere but a
`ref` parameter (a read-only borrow) or an `own` parameter (a move, which the
resource snapshot already rejects when another task also uses the value). A
capture reaches storage when a field, tuple element or type argument does;
fields come from the class field model or, for zones and other hosts, from the
same families the host field typing reads. An
unresolved field type, method or unmodeled node counts as a write, so a gap
rejects rather than admits. `parallel_capture_reach_smoke.sh` (Linux push
shard) holds eleven race rejections (two on zones), the own-move conflict
that stays with the resource snapshot, and five admitted programs, which run
on both native backends; the full backend compare still passes.

Not covered yet: an enum payload that holds a collection, a function-typed
capture whose closure holds one, and a nominal read through `Option<Subject>`.
The default routes refuse every program in this set; they do not compile these
`parallel` shapes yet.

### Intent: a semantic race the checks admit

The 2026-09-24 architecture review asked for the smallest program where two
intents write different places and still break one business rule. It exists
and runs on both native backends:

```pergyra
intent ReserveOne(booking: Booking, seat: Seat, otherFree: Bool) {
    concurrent;
    step reserve {
        using: booking;
        who: seat;
        authorized by: seat;
        pre: otherFree;
        on: seat.Reserve();
        expect: seat.reserved;
    }
    success: seat.reserved;
    failure: false;
}

// Rule: at most one of a and b is reserved.
let aFree: Bool = !a.reserved;
let bFree: Bool = !b.reserved;
parallel {
    { okA = ReserveOne(za, a, bFree); }
    { okB = ReserveOne(zb, b, aFree); }
}
```

Both intents succeed and both seats end up reserved (`true true true`), on
native C and native LLVM. With `exclusive` instead of `concurrent` the result
is the same. Run one after the other, reading the other seat each time, the
second intent is refused by its `pre:` clause.

Nothing reports it, for three reasons:

- The parallel capture checks see disjoint bindings (`a` in one task, `b` in
  the other) and two reads of primitive `Bool` snapshots, which docs/178
  admits as Copy evidence.
- The runtime admission (`pgy_intent_enter_export`) compares subject
  identity. The two intents name different seats, so neither `exclusive`
  nor `concurrent` makes them conflict.
- The rule itself, one reservation across both seats, has no place in the
  program. Step `invariant:` and `pre:` clauses are checked inside one
  intent, against values that intent was handed.

Reading the other seat live instead of through a snapshot does not compile:
one task writes the seat the other reads, and the capture check rejects the
read-write race. So inside `parallel` the only way to this race is a stale
observation. That locates what is missing. A precondition that reads a
parallel snapshot names no logical resource and no revision, and the commit
does not check that the observation still holds. The review's list
(`LogicalResourceId`, observed revision, precondition, commit rule) maps onto
those three gaps. Closing them is a language decision (where a logical
resource is declared, what a commit revalidates, and whether a snapshot may
feed `pre:` at all), so this round records the program and makes no change.

Also found while building it: native refuses `Booking(a)` for a subject `a`
("implicitly copies subject binding 'a' into slot 'seat'") and asks for
`Booking(Clone(a))`; the default C route accepts `Booking(a)`.

## Fourth round, 2026-09-24: against the whole-repository reproduction

`docs/audits/2026-09-24_whole_repository_redteam_reproduction.md` and the GUI
audits of the same day were observed on `e48ca632` with another lane's
uncommitted work and, for the GUI, an older Windows build. Each finding was
rerun on a clean build of `761531dc`:

| Finding | On clean `761531dc` | Outcome |
|---|---|---|
| R1 `Array<T>` value copy aliases storage | reproduces on native C, native LLVM, default C | collection lane (M3) |
| R2 shallow `Array<String>` accepted | reproduces on all three | collection lane |
| R3 `Unwrap(Err)` succeeds | does not reproduce | repaired in the second round |
| R5 Float `ToString` precision | default C only | the other lane's pending formatter work (A1) |
| `own` enum consumed twice | accepted on all three | native repaired, see below |
| contextless `Some`: C accepts, LLVM refuses | reproduces | open, see below |
| generic unification diagnosed in MIR | reproduces | repaired, see below |
| channel starvation gate passes without LLVM | script behavior | repaired |
| GUI `ToString(Bool)` prints `1/0` on C | does not reproduce | repaired by `c4162629` (match-bound payload typing); pre-fix builds print `edit 1/0` |
| SoT row gate, 49 unreachable gates, stale handoff and census | the gates pass on the clean tree | state of the other lane's uncommitted work |
| optional capabilities, `ToInt` | unchanged | decisions (PP-024, ToInt contract) |

Repaired in this round:

- **`own` enum double consume.** An enum whose payloads are not all copy-only
  is now tracked like a struct, so a second `own` use is refused. The default
  route still has no move tracking for `own` (M11).
- **Generic binding.** Native semantic refuses a call that binds `T` to two
  incompatible types or leaves it unbound; `generic_falsification_smoke.sh`
  had run on the default route since that route changed and was red, and now
  runs natively in the push shard.
- **Unwrap operands evaluated twice on native C (found here).**
  `UnwrapOption(Next())` ran `Next` twice, and `UnwrapOr` evaluated its
  fallback lazily where LLVM evaluates it. Both backends now evaluate each
  operand once (`unwrap_operand_once` in the backend compare).
- **Native LLVM dropped match guards (found here).** `case Circle(r) if r > 10`
  matched every `Circle`; fixed in `761531dc`.

Found and left open:

- **Contextless `Some` on LLVM.** C types `Some(x)` from its payload, so it is
  not anonymous; LLVM needs a consumer type and refuses `IsSome(Some(5))`,
  `let x = Some(5)` and `Some(5)` in a condition. Without a consumer it also
  borrows the enclosing function's return type for the layout. The fix is for
  LLVM to take the expression's semantic type, not to refuse these programs.
- **`Option<Subject>`.** `Some(card)` for a subject parameter fails in code
  generation on both native backends, with or without generics.
- **MIR generic binder.** `TakeOpt(Some(5))` for `TakeOpt<T>(o: Option<T>)`
  fails in MIR lowering on both native backends because the binder matches
  type-name text instead of consuming the semantic binding;
  `generic_nested_failclosed_smoke.sh`, outside CI, has been red since.
- **Default route coverage.** All 944 backend-compare cases were compiled on
  the default C route and compared with native C: 751 print the same output,
  5 differ (all Float formatting, R5), and 188 are refused. About 30 of the
  refusals end with the driver exiting and no diagnostic, and 8 fail in the C
  compiler or linker instead of before code generation.

## Fifth round, 2026-09-24: review of `e48ca632..e196e1c5`

The round's own changes were reviewed on a WSL build of `e196e1c5`. The
push run for `e196e1c5` (35975030140) passed every job except build-linux,
which was cancelled at its 30-minute limit. The limit is repaired below.

Repaired, each with a row that fails if it returns:

- **Parallel reach.** Two tasks that each built their own `Seat(1)`, or
  wrote `Array<Int>` in a local's annotation, were refused as a race: the
  type name was looked up as a captured binding. A `ref` argument was never
  counted as a write, although the callee could call a method that writes
  it. An enum payload holding an `Array` was not followed. A
  self-referential type recursed into the depth limit. Rows in
  `parallel_capture_reach_smoke.sh` and a semantic unit test.
- **Result consumers on native C.** A consumer took its specialization from
  the enclosing return or expected type. Once `b31cbdd0` made `UnwrapOr` a
  typed function, `UnwrapOr(r, 7)` on a `Result<Int, String>` inside a
  function returning `Result<Bool, String>` stopped compiling
  (`result_consumer_operand_type`).
- **IoError in hosted bodies.** Native C emitted the `Result<_, IoError>`
  specializations after the class and subject bodies that use them. Native
  LLVM could not type `CharFromCode`, `TryReadFile` or `UnwrapOption` inside
  a method or an action; the `UnwrapOption` case predates this range
  (`io_result_in_hosted_bodies`).
- **A program's own IoError on the default route.** The struct form died in
  C emission. The enum form was refused as `match_pattern_invalid` with
  `Span: none`. Both are now refused as `builtin_type_name_taken` at the
  declaration (`io_result_builtin_owner.sh`).
- **`self.n + 1;` in a method body** reached MIR on the default route, and
  an unclosed `match` was reported at the end of the file
  (`default_route_diagnostic_position_owner.sh`).
- **Location join.** Parser rows that do not join the tree used to print
  `Span: none` for every later diagnostic. The parse now stops there. A
  default-route sweep of the backend-compare corpus found the one source:
  the compact `parallel { }` body recorded a Block row with no `Block:` line
  in the tree, so every diagnostic in a program with a parallel block had no
  position. Seven corpus programs were affected.
- **`UnwrapOr` on the default C route** called the fixed Result<Int> helper,
  so a `Result<Int, String>` operand failed in the C compiler. Each Result
  specialization now defines its own (`builtin_surface_parity_owner.sh`).
- **Gates.**
  - Five default-route gates ran native legs whenever `PGY_NATIVE_PIPELINE`
    was exported.
  - The containment gate accepted any default LLVM refusal of the declared
    forms.
  - The position gate could take a code and a span from two different
    diagnostics.
- **CI budget.** The last green build-linux took 29 minutes. Core shard step
  6 depended on `self-host-compiler`, so it rebuilt DRV-2 for 14 minutes on
  top of the admitted pair. The step now runs its script on the admitted
  pair, and `self_host_ci_profile_smoke.sh` refuses a core-shard target that
  depends on `self-host-compiler`.

Found and left open:

- **Bare exits in declaration parsers.** `93d1d9a8` says every
  default-route diagnostic has a code and a position. The intent, zone and
  effect/relation parsers still end about 90 refusals with a bare `Exit(1)`.
  `zone_effect_pool_runtime` and `role_include_methods` print only
  `self-host driver failed (exit 1)` on the default route, and
  `relation_effect_projection_sync` prints an uncoded `PARSE ERROR`.
- **Native line for a struct IoError.** Native names `dup.pgy:1` for
  `enum IoError` but `:0` for `struct IoError`.
- **IoError variant names.** The builtin variants share the enum-variant
  namespace, so a user enum with a variant named like one of them collides.
- **A `for` loop with an early return** (`for x in xs { if x == v { return
  true; } }`) passes the self-host semantic checker, then fails in MIR on both
  default legs. Not attributed to this range.
- **Default LLVM JSON receipts.** Any direct-MIR codegen refusal under
  `--error-format=json` prints `self-host JSON diagnostic receipt is
  malformed`.
- **Self-host shard rebuild.** The self-host contract shard also rebuilds
  DRV-2 once, 12 minutes, in its first step. It runs in 25 minutes, inside
  its budget.
- **Channel send on the default route.** `ch <- 10;` as a statement ends in
  `AST node is outside bounded MIR producer` with no code. The builtin
  surface gate already records that the default route has no channel
  runtime.

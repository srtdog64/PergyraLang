# Red-Team Audit: Ownership, Runtime Memory and Arithmetic Legs

Status: READ-ONLY FINDING — REPRODUCED, NOT REPAIRED, except the items listed
under "Repaired in this round".

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
commit yet (the map runtime headers, `pgy_runtime_panic_contract.h`,
`option_result_runtime_owner.pgy`, `runtime_call_rewrite_owner.pgy`). Repairs
there need that lane's agreement first.

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

# B. A generic by-value parameter returned inside an Array

Read-only research, 2026-11-16. Nothing was built or run; statements about
compiler behaviour come from reading source at `ec109c765` and from the
evidence file `docs/truth/evidence/2026-11-16_weekly_run_43747258112.md`
(present on `origin/main` at `704c32d60`, not on this branch).

## 0. What the source says today

- **Native checks a generic body once**, with `T` opaque (`TYPE_KIND_GENERIC`).
  Specialisation happens in codegen (`src/codegen/llvm_generic_method_specialization.c`
  and the member-call specialisers), after the checker.
- `semantic_classify_ownership_type` (`src/semantic/type_checker_ownership_classify.c:57`)
  classifies an opaque `T` as `BORROW_TRACKED`: the conservative class.
- `type_is_general_boundary_type` (`src/semantic/type_checker_helpers_resources.c:211`)
  is movable handle, subject, or value nominal. An opaque `T` is none of
  them, so `own value: T` fails the mode contract in
  `type_checker_func_param_contract.c:230`. A comment in
  `type_checker_ownership_call.c:417` says this was deliberate.
- The refusal is at `type_checker_ownership_param_summary.c:447-471`. An Array
  result plus a `BORROW_TRACKED` default formal with a return escape needs one
  of three proofs. All three miss this shape:
  1. `semantic_array_storage_default_formal_result_current` wants every return
     to be the formal itself (`ARRAY_RESULT_DEFAULT_FORMAL`). `[value]` is
     `ARRAY_RESULT_LITERAL`.
  2. the return-copy landing and 3. the String store landing put a `Clone` in
     the callee. `Clone` of an opaque `T` counts as a copy by value only
     (2026-10-22), so nothing lands. That is also why `[Clone(value)]` is refused.
- The move rule itself already covers `T`:
  `semantic_default_aggregate_formal_moves` (`default_aggregate_move_rule.c:35`)
  returns true for any non-Array default formal whose summary is more than a
  read, and the caller side (`default_aggregate_move_owner.c`) sees the
  concrete argument type. The gap is the callee-side proof and the String
  instantiation, which the aggregate move rule leaves to the String owners.
- Self-host specialises in MIR lowering (`src/self_hosted/mir_lower/generic_specialization_fact_owner.pgy`,
  `generic_instance_plan_owner.pgy`). **Unknown:** whether its semantic owners
  prove this return or simply have no check for it. The ASan-clean runs in the
  evidence do not cover a caller that reads the argument again.

## 1. Survey

| Language | Where ownership of `T` is decided | Invariant | Premise |
|---|---|---|---|
| Rust | At the definition, once. `fn wrap<T>(v: T) -> Vec<T>` moves `v`; copying needs a `T: Clone` bound and a written `.clone()`. Monomorphisation is codegen only. | A generic body is valid for every `T`, so it may only move, drop or pass `T`. | By-value means move for every type; `Copy` is opt-in per type. |
| Swift 5.9-6 | At the definition. Default convention is borrow for function parameters, consume for initialiser parameters; storing a borrowed copyable `T` inserts a copy (retain). `consuming`/`borrowing` are optional for `Copyable`, mandatory for `~Copyable` `T` (SE-0377, SE-0427). | Every `T` is `Copyable` unless the bound is suppressed, so an implicit copy is always available and always whole. | Reference counting or witness-table copy makes "copy" cheap and uniform. |
| Hylo | At the definition. `let` (default), `inout`, `sink`, `set`. Putting a parameter in a returned container needs `sink`; a `let` argument needs an explicit `.copy()` and a `Copyable` bound. | No implicit copy anywhere; a value has one owner. | The programmer writes the convention. |
| Mojo | At the definition. `read` (default), `mut`, `var`. A `var` argument is moved with `^` or implicitly copied if the type is `ImplicitlyCopyable`; otherwise `.copy()` is written. | Callee convention is in the signature; the caller chooses move or copy. | Implicit copy only for types that declare it. |
| Koka (Perceus) | Per definition, with uniform representation. The compiler inserts `dup`/`drop`; parameters are owned, a later use becomes a `dup`. | Polymorphic code needs no ownership annotation because every boxed value can be `dup`ed in O(1). | Reference counts on every heap value. |
| Linear Haskell, OxCaml modes | Ownership polymorphism: a function is polymorphic in multiplicity or mode (`a %m -> b`; uniqueness/affinity axes). | One definition is checked for all modes. | Mode variables in types; Jane Street reports the polymorphism and annotation burden as still open. |
| Lobster | Ownership inferred per function, combined with type specialisation and a runtime refcount fallback. | No annotations; unproved cases keep a refcount. | A runtime fallback exists. Whether specialisation is keyed on ownership was not confirmed. |
| Carbon | Checked generics: body type-checked once against the constraints. | Same as Rust for moves. | Design still in progress. |

Two findings matter here.
1. Every language without a refcount fallback decides at the **definition** and
   treats "by-value `T` stored in the result" as a **move (consume)**. The
   reason is parametricity: with no bound, a body can only move, store or
   forward a `T`, so the escape summary computed with `T` opaque is exact for
   every instantiation.
2. The copy for a caller that keeps using the value is always decided at the
   **call site**, where the type is concrete (Mojo implicit copy, Swift
   retain, Rust `.clone()`). No surveyed language copies an opaque `T` inside
   the generic body without a bound that guarantees a whole copy.

Sources (verified by search today unless marked *memory*):
- SE-0377 "borrowing and consuming parameter ownership modifiers", Swift Evolution, 2023. https://github.com/apple/swift-evolution/blob/main/proposals/0377-parameter-ownership-modifiers.md
- SE-0427 "Noncopyable Generics", Swift Evolution, 2024. https://forums.swift.org/t/se-0427-noncopyable-generics/70525
- Racordon, Shabalin, Zheng, Abrahams, Saeta, "Implementation Strategies for Mutable Value Semantics", Journal of Object Technology, 2022. https://research.google/pubs/pub51426/
- Mojo Manual, "Ownership", Modular, 2025-2026. https://docs.modular.com/mojo/manual/values/ownership
- Lorenzen, White, Dolan, Eisenberg, Lindley, "Oxidizing OCaml with Modal Memory Management", ICFP 2024. https://icfp24.sigplan.org/details/icfp-2024-papers/19/Oxidizing-OCaml-with-Modal-Memory-Management
- van Oortmerssen, "Memory Management in Lobster". https://aardappel.github.io/lobster/memory_management.html
- *memory*: Reinking, Xie, de Moura, Leijen, "Perceus: Garbage Free Reference Counting with Reuse", PLDI 2021. https://www.microsoft.com/en-us/research/publication/perceus-garbage-free-reference-counting-with-reuse/
- *memory*: Bernardy et al., "Linear Haskell", POPL 2018. https://arxiv.org/abs/1710.09756
- *memory*: The Rust Reference, "Generic parameters". https://doc.rust-lang.org/reference/items/generics.html
- *memory*: Carbon design, "Generics: Overview". https://github.com/carbon-language/carbon-lang/blob/trunk/docs/design/generics/overview.md

## 2. Options

| | Soundness | DX cost | Compile time | Size (native / self-host) |
|---|---|---|---|---|
| **(a) Move rule on `T`** | Sound if the call-site obligation is discharged at the concrete type (see below). Callee summary is exact by parametricity. | None. No annotation; fresh values and last uses cost nothing; a later use copies or is refused with the callee and parameter named. | One summary per generic definition (already computed) plus one check per call. | Small-medium / unknown. Native: one new proof arm for "literal or constructor element is a moving default formal", the String instantiation at the call, and no callee release of the moved formal. Self-host: depends on the unknown in section 0. |
| (b) Per instantiation | Sound, most precise (`T = Int` needs nothing, `T = String` can land a callee `Clone`). | None. | Body ownership analysis times the number of instantiations; the analysis is already the slowest phase. | Large in both. Native has no checker-level substitution; the summary store is keyed by declaration, not by instance. |
| (c) `own` on `T` | Sound (it is (a) written down). | One annotation per generic container-builder: the user discipline the 2026-10-18 decision rejects. `own` on `T = Int` is a no-op the user must still write. | Same as (a). | Small / small, but needs (a)'s callee proof anyway. |
| (d) Explicit `Clone` only | **Not sound at the definition.** `Clone(T)` is whole for Int, String and Array of scalars, but shallow for structs and deeper Arrays. Admitting `[Clone(value)]` once admits the shallow instantiations. Sound only with (b). | A `Clone` in every generic builder, and a real copy on every call even for a fresh argument. | Same as (b) if made sound. | As (b). |
| (e) Keep refusing | Sound. | No generic function can build a container from its argument; the two manifest fixtures stay refused and the driver parity gate stays red, or the fixtures are deleted and the self-host admission becomes an unexplained divergence. | None. | A negative fixture in both front ends; self-host must then refuse too. |

Why (a) is sound with one check of the body:
- The callee fact is "formal `value` reaches the result". With `T` opaque and
  unbounded the body cannot read through `value`, so the fact holds for every
  instantiation.
- The obligation falls on the caller, which sees the concrete type:
  - copy-only (`Int`, payload-free enum): nothing shares, nothing moves;
  - aggregate (struct, Array, collection): the existing move, then the
    2026-10-22 copy if the caller reads it again and `Clone` is whole, else
    the existing refusal;
  - `String`: the argument is handed over as an owned String element. A
    literal or a String the caller still reads is copied at the call (the
    existing String boundary copy); a last use of an owned String moves;
  - handle, subject, `Future`, `Channel`: existing storage-site rules decide;
    this proposal admits none of them;
  - the caller's own generic formal: the fact propagates through the caller's
    summary, so the caller's formal moves in turn.
- The callee must not release the moved formal. Its cleanup comes from the
  same fact.

## 3. Recommendation

**(a)**, with the obligation discharged at the call site. Keep (c) refused
for now: it adds a spelling for something the compiler infers. `[Clone(value)]`
in a generic body is treated as `[value]` (a copy by value does not discharge
the move). (b) is the later precision step if a real program needs a
callee-side copy.

Record of ceremony: none added. No annotation, no source change to the fixtures.

### Draft decision file

`docs/truth/decisions/2026-11-16_open_generic_formal_returned_in_container_moves.md`

```markdown
# 2026-11-16 (OPEN): does a generic by-value formal returned inside a container move?

Raised by: the Claude lane. Needs the user's decision.

## Question

    func Wrap<T>(self, value: T) -> Array<T> { return [value]; }

Native refuses this ("Array return still carries backing or element
lifetime from non-own parameter 'value'; inferred transfer proof is
unavailable"), refuses `own value: T` and refuses `[Clone(value)]`. The
self-host admits all three. No generic function can return a container
built from its argument.

## Proposed rule

The [default aggregate move](2026-10-18_default_aggregate_move.md) applies
to a default-mode formal of generic type `T`. When the callee stores the
formal in a value it returns, the call moves the argument.

- The callee is checked once, with `T` opaque. Its summary records that the
  formal reaches the result; the callee does not release it.
- The caller applies the move at the argument's concrete type: a copy-only
  value is not moved; an aggregate moves, and a later use copies it under
  [2026-10-22](2026-10-22_moved_argument_used_again_is_copied.md) or stays
  refused; a String is handed over as an owned element and is copied at the
  call when it is a literal or is read again.
- A `Clone` of an opaque `T` is a copy by value. It does not replace the move.
- There is no annotation. `own` on a generic `T` stays refused.

## Must stay refused (falsifiers)

1. Same formal twice: `func Pair<T>(v: T) -> Array<T> { return [v, v]; }`.
2. Borrowed formal: `func WrapRef<T>(ref v: T) -> Array<T> { return [v]; }`.
3. Shallow instance read again: `struct Rows { ids: Array<Int>; }`,
   `let r = w.Wrap(rows); Log(ToString(ArrayLength(rows.ids)));`.
4. Deeper Array read again: `let xs: Array<Array<Array<Int>>> = ...;
   let r = w.Wrap(xs); Log(ToString(ArrayLength(xs)));`.
5. Used after a move inside a generic body:
   `func Both<T>(self, v: T) -> Array<T> { let a: Array<T> = self.Wrap(v); return [v]; }`.
6. A handle instance: `w.Wrap(future)` and `w.Wrap(channel)`.

## Must run clean under ASan on C and LLVM, native and self-host

7. `let s: String = Concat("a", "b"); let r = w.Wrap(s); Log(s); Log(r[0]);`
   MIR shows one `Clone` at the call; the String outlives neither owner.
8. `let r: Array<String> = w.Wrap("literal");` The released element is not
   the static literal.
9. `let xs: Array<Int> = [1]; let r = w.Wrap(xs); ArrayPush(xs, 2);
   Log(ToString(ArrayLength(r[0])));` prints 1.
10. The two manifest fixtures, unchanged.

## Options not taken

- Check per instantiation: precise, but multiplies the ownership analysis
  by the number of instances and needs checker-level substitution.
- `own` on `T`: an annotation for a fact the compiler infers.
- Explicit `Clone` only: not sound when the body is checked once.
- Keep refusing: sound; the self-host would then have to refuse as well.

Replaces: nothing until decided.
```

## 4. Owners and gates

Native (fact producer first):
- `src/semantic/default_aggregate_move_rule.c`, `default_aggregate_move_owner.c`:
  already move a generic formal; add the String instantiation at the call
  (owned hand-over or copy) and confirm copy-only instances are skipped.
- `src/semantic/array_storage_result_owner.c`: accept a return whose literal
  or constructor element is a moving default formal as a current transfer.
- `src/semantic/type_checker_ownership_param_summary.c:447-471`: last
  consumer; no new exemption, it reads the proof above.
- Cleanup synthesis and the generic specialisers under `src/codegen/`:
  the callee instance does not release the moved formal.
- Unchanged: `type_checker_func_param_contract.c` (`own T` stays refused),
  `type_checker_ownership_classify.c`.

Self-host: first settle the unknown (proof or missing check) by running
falsifiers 1-6 through the installed driver. Then the same rule in
`src/self_hosted/semantic/array_storage_element_lifetime_owner.pgy`,
`array_storage_call_preservation_owner.pgy` and the
`ast_collection_formal_effect_*` owners; instance cleanup in
`src/self_hosted/mir_lower/generic_specialization_fact_owner.pgy`.

Gates:
- `pgy --test-native-mir-json-oracle` over the 285 manifest fixtures (the
  two generic fixtures admitted), then the driver parity shard that failed
  in run 43747258112;
- falsifiers 1-6 as native negative fixtures and as self-host
  semantic-parity invalid fixtures;
- programs 7-10 as runtime-equal C/LLVM runs under ASan on both compilers;
- `function-param-flow-summary-test-smoke`; DRV-2 refusal count unchanged.

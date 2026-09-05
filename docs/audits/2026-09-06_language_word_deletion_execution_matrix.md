# Language-word deletion experiments: executed matrix — 2026-09-06

Status: `EXECUTED SOURCE PAIRS ON TWO PIPELINES; NO WORD REMOVED; NO SEMANTIC AUTHORITY`

Observed base revision: `91119c45683c10efdccaad18c95fdc2a9ebf0d41`.
Compilers: `bin/pgy.exe` (sha256 `0f9f4f30255d6850…`, built 2026-09-05 06:30)
delegating to `bin/pgy-self-driver.exe` (sha256 `7928ed6be2d38a9c…`, built
2026-09-05 14:45). Neither binary is asserted to be rebuilt from the base
revision; the driver is the installed DRV-2 of the previous day.

This report executes the matrix that
[the intake](2026-09-06_language_word_deletion_intake.md) recorded as missing:
every source pair is a repository fixture, every pair was compiled and, when it
compiled, run, and every pair was checked on both installed pipelines. It uses
the intake's four verdicts and its evaluation contract without change. It does
not own language semantics, a registry row, self-host progress or the next
compiler rung. Equal stdout for one pair is evidence about that pair, not a
theorem about the word.

Follow-up qualification: the [value-receiver/surface review](2026-09-06_class_receiver_surface_review.md)
independently tests explicit class `inout self` and falsifies a global no-op
interpretation of row 26. Native unsafe blocks record an effect and are rejected
in parallel bodies; equal output of the original sequential pair remains true.
The runner below collects observations but does not compare them to a baseline;
exit zero alone is not evidence that earlier recorded outcomes reproduced.

## 1. Method

Fixtures: `tests/concept_semantics/word_deletion/cases/<experiment>/`, 35
experiments, 104 programs. Each experiment holds up to four roles:

| Role | Meaning |
| --- | --- |
| `orig.pgy` | the program written with the word under test |
| `subst.pgy` | the same program written without it |
| `neg_orig.pgy` | the same mistake written with the word; expected to be rejected |
| `neg_subst.pgy` | the same mistake written without the word; the question is whether it is still rejected |

Extra probes (`orig_where.pgy`, `pre.pgy`, `class_param.pgy`, …) are recorded
individually. `tests/concept_semantics/word_deletion/run_matrix.py` compiles
each program with the C backend, runs the executable, and stores compile exit
code, diagnostic tail, stdout and run exit code. The matrix ran twice:

```sh
python3 tests/concept_semantics/word_deletion/run_matrix.py                       # public path: bin/pgy -> installed self-host driver
PGY_EXTRA_FLAGS=--native-pipeline PGY_RESULTS=native.json \
    python3 tests/concept_semantics/word_deletion/run_matrix.py                   # native front end
```

The evaluation-order probe (`33_eval_order`) was additionally compiled with
`--backend=llvm` on the native pipeline. Results live under
`.tmp/self_hosted/word_deletion/`; they are evidence, not owners.

Two experiments had to be corrected during the run and the corrections are
themselves findings: a parameter named `fail` is a parse error because `fail`
is reserved, and any pair that read a mutated value in the same expression as
the mutating call was confounded by evaluation order (section 2.2). All pairs
below sequence the call and the read in separate statements.

## 2. Findings that precede any word verdict

### 2.1 The public path and the native front end disagree on 34 of 104 programs

The word verdicts in section 4 are stated against the native front end,
because that is where the admission rules live. The shipped public path
(`bin/pgy` delegating to the installed self-host driver) does not carry them:

| Program | Native front end | Public path |
| --- | --- | --- |
| `04_match_enum/neg_orig` (arm `Rect` missing) | rejected: non-exhaustive match, fallthrough | accepted, runs, prints `0` |
| `05_let_mut/neg_orig` (write to `let` field) | rejected: field is immutable | accepted, prints `5` |
| `10_object_struct/neg_orig`, `neg_subst` (write to `object` / `let` field) | rejected | accepted, prints `9` |
| `13_spawn_await/neg_orig` (handle never awaited) | rejected: handle not retired at fallthrough | accepted, prints `spawned` |
| `15_caps/neg_orig`, `neg_caller` (declared `caps clock`, body prints) | rejected: missing declared capability `io_write` | accepted, prints `hi` |
| `21_ability_role/neg_orig` (`requires Ready`, no role impl) | rejected | accepted, prints `1` |
| `06_action_func/orig_within` (`within` zone without `authority`) | rejected: zone has the slot but no authority contract | accepted, prints `0` |
| `35_subject_param_alias/orig` (subject passed to a function, mutated by method) | prints `90`: reference semantics | prints `100`: copy semantics |
| `07_intent_compensate/subst`, `neg_subst` (same shape as above) | `90` / `80` | `100` / `100` |
| `28_intent_header_policies/retry` | rejected: retry lowering not implemented | accepted, prints `false 0` |
| `35_subject_param_alias/subst` (`inout acct: Acct`) | C compile error: `Acct_Debit(Acct *self)` receives `Acct` | accepted, prints `90` |
| `12_guard_family/pre`, `invariant` | run | self-host driver exits 1 with no diagnostic |
| `14_parallel_join/*`, `17_zone_authority/*`, `18_event_causes/orig`, `19_party_roster/orig`, `22_dyn_bind/orig`, `23_extern/*`, `25_slot_own_ref/orig`, `26_unsafe_block/orig`, `27_loop_while/orig`, `28_intent_header_policies/orig`, `29_effects_clause/*`, `30_object_projection/orig` | run | not compiled: semantic `initializer_type_unresolved`, `zone frontier pass limit is invalid`, `MIR instruction expression graph is missing`, parse errors for `unsafe {` and effect `func` bodies, or `Now` undeclared in emitted C |

Two consequences for this audit. First, every "guarantee retained" verdict in
section 4 is a statement about the native checker only; on the shipped path
the negative program compiles and runs. Second, the identity axis that
separates `subject` from `class`/`struct` (a subject passed to a function is
the same object) is observable only on the native pipeline; the public path
copies it. `tests/concept_semantics/README.md` already names the open
source-admission parity claim; this table is its executed extent for these 35
shapes.

### 2.2 Operand evaluation order is unspecified and differs between backends

```pergyra
subject Box { let mut n: Int; func Bump(self) -> Int { n = n + 1; return n; } }
func Main() -> Void {
    let b: Box = Box(0);
    Log("bump=" + ToString(b.Bump()) + " n=" + ToString(b.n));
}
```

| Pipeline / backend | Output |
| --- | --- |
| native, C | `bump=1 n=0` |
| native, LLVM | `bump=1 n=1` |
| public, C | `bump=1 n=0` |

The C backend lowers the concatenation to nested runtime calls whose argument
evaluation order C leaves unspecified; the LLVM backend evaluates left to
right. `docs/180_compiler_logical_spine_handles_gates.md` already states for
array-literal elements that "a C initializer whose evaluation order is not the
language contract" must be sequenced; the same rule is not applied to binary
operator operands. This is a runtime-equal parity violation between the two
native backends and a missing language rule. Fixture: `33_eval_order`.

### 2.3 A `class` method's write to `self` is lost without a diagnostic

`32_class_mutation/orig`: `class Counter { let mut n: Int; func Bump(self) -> Void { self.n = self.n + 1; } }`,
two calls, then `c.n` prints `0` on both pipelines. The same body under
`subject` prints `2`. If a class is a value type by design, the assignment to
`self.n` should be rejected; if not, the copy is a defect. Either way the
program compiles and does nothing, which is hidden control flow. Fixtures:
`11_class_struct`, `32_class_mutation`, `35_subject_param_alias/class_param`.

### 2.4 Three registered words cannot appear in any accepted program

| Word | What happens |
| --- | --- |
| `timeout`, `backoff` | parser: "'timeout'/'backoff' resilience modifiers are reserved but not implemented" |
| `retry` | parsed and carried to MIR; native checker: "execution lowering is not implemented"; public path accepts and runs the intent once |

Fixture: `28_intent_header_policies/{retry,timeout_reserved}`.

### 2.5 `guard`, `expect` and `post` are one mechanism; `pre` and `invariant` are another

`12_guard_family` runs one intent step whose action increments `calls` on the
participant, with exactly one failing clause each time (`where`, `using` and
`who` present):

| Clause set to `false` | Action ran? | `IntentHistoryStepFailure(0)` |
| --- | --- | --- |
| `guard` | yes (`w=1 z.w=1`) | `guard:S` |
| `expect` | yes | `expect:S` |
| `post` | yes | `post:S` |
| `pre` | no (`w=0 z.w=0`) | `pre:S` |
| `invariant` | no | `invariant-pre:S` |
| none (`expect: true`) | yes, `ok=true` | empty |

The three post-action checks leave identical state and differ only in the
failure label. `guard` is therefore a post-check despite its name. `pre` and
`invariant` block execution; `invariant` is checked before and would also be
checked after.

## 3. Executed pairs

Verdicts use the intake vocabulary: **spelling removable** (same fact
retained), **behavior replaceable** (a current-language encoding exists but a
rejection, alias, phase or cost differs), **no same-guarantee replacement
found** (bounded negative search), **unverified**. "Native" and "public" give
the pipeline outcomes; `SAME`/`DIFF` compare `orig` and `subst` stdout.

| # | Word(s) | Native | Public | Verdict and the boundary that decides it |
| --- | --- | --- | --- | --- |
| 01 | `defer` | SAME; `neg_subst` (one exit path without cleanup) accepted | same | behavior replaceable, guarantee lost: the missing cleanup is not detectable without `defer` |
| 02 | `inout` | SAME; `neg_orig` (mutating a value parameter) rejected; `neg_subst` (forgot to reassign the returned array) accepted | same | behavior replaceable, guarantee lost; the rewrite adds a copy and a caller reassignment |
| 03 | `for … in a..b` | SAME (7 -> 9 loc) | same | spelling removable in this shape; convenience, no independent guarantee tested |
| 04 | `match`/`case` | `subst` rejected: `is` supports Int/Long/Float/Bool only; `neg_orig` (missing arm) rejected | `neg_orig` accepted, prints `0` | no same-guarantee replacement found: enum payload access has no other syntax |
| 05 | `let mut` | SAME with local `mut` dropped; `neg_orig` (write to `let` field) rejected | `neg_orig` accepted | local `mut`: spelling removable (not enforced); field `mut`: guarantee retained |
| 06 | `action` vs `func` | SAME for a plain state change; `func … within` is a parse error; `action … within` on a zone without `authority` rejected | `orig_within` accepted | behavior replaceable for plain mutation; `within`/`authorized by` need `action` |
| 07 | `intent`, `step`, `compensate`, `success`, `failure` | SAME (`90`/`90` on success and failure); `neg_subst` (undo forgotten) accepted, prints `80` | `subst` differs (`100`) because of 2.1 | behavior replaceable, guarantee lost: automatic compensation has no substitute check |
| 08 | `where:` with `using:` | SAME; `where: OtherZone` against `using: arena` rejected | same | spelling removable when `using` is present; conflict detection retained |
| 09 | `transfer: a -> b` vs `move a to b` | SAME (`cart=5 pay=7 b=7`) | same | spelling removable: two spellings of one step fact |
| 10 | `object` vs `struct let` | SAME; both writes rejected | both writes accepted | for read-only use the guarantee is identical; `object`'s remaining value is the projection context (30) |
| 11, 32 | `class` vs `struct`+`func` | DIFF: class `self.n` write lost (2.3) | same | behavior replaceable; the class form is the defective one in this pair |
| 12 | `guard` `expect` `post` `pre` `invariant` | see 2.5 | `pre`, `invariant` fail silently | `guard`/`expect`/`post`: spelling removable down to one; `pre`/`invariant`: distinct phase |
| 13 | `spawn`/`await`/`async` | SAME vs direct call; unawaited handle rejected | `neg_orig` accepted | behavior replaceable only by giving up concurrency; the lifecycle rejection is the guarantee |
| 14 | `parallel (x in xs) join with sum` | SAME vs sequential fold (`91`); write to outer `acc` rejected; `join with max` prints `9` | not compiled | behavior replaceable by sequential code only; race rejection is the guarantee; `sum/max/…` are fold selectors |
| 15 | `with caps` | SAME with the clause dropped (caps inferred); declared `clock` while printing rejected, in callee and caller | `neg_*` accepted | the clause is a declared upper bound; its check is a guarantee on the native path |
| 16 | `zone` + method vs `struct` + `inout` func | SAME | same | behavior replaceable for one slot and one method; authority/binding not exercised here |
| 17 | `authority`, `apply … by` | SAME; `apply … by observer` rejected only when `authority owner` is declared | zone does not compile | no same-guarantee replacement found: without `authority` the wrong approver is accepted |
| 18 | `event`, `+=` | SAME vs direct call | not compiled | behavior replaceable for one handler; multi-subscriber and `causes` untested |
| 19 | `party`, `roster`, `shared` | SAME vs structs and free functions (14 -> 9 loc) | not compiled | behavior replaceable in this shape; `dyn role slot` is the part with no struct form (22) |
| 20 | `vessel` | SAME vs a struct field | same | behavior replaceable; no separate guarantee observed |
| 21 | `ability`, `role`, `impl`, `requires` | SAME vs a Bool field; `requires` without a role impl rejected | `neg_orig` accepted | behavior replaceable, guarantee lost: the requirement becomes a runtime branch |
| 22 | `dyn role slot`, `bind` | SAME vs enum + `match` | not compiled | behavior replaceable; every method needs its own `match`, so the burden grows with the ability |
| 23 | `extern "c"` | SAME vs the `Now()` builtin | not compiled | no same-guarantee replacement found for an arbitrary C symbol; the builtin only covers this one |
| 24 | `as` | `subst` fails: no Float/Int conversion builtin (`ToInt` takes a string) | same | no replacement found for numeric casts |
| 25 | `slot`, `own`, `ref` | SAME; borrow after `Release` rejected; the value-typed rewrite accepts the same order | not compiled | behavior replaceable, guarantee lost: no use-after-release check without slots |
| 26 | `unsafe { }` | SAME | not compiled (parse error) | equal sequential output only; the follow-up parallel refusal disproves a global no-op/removable verdict |
| 27 | `loop { }` | SAME vs `while true` | not compiled | spelling removable: the parser desugars it to `while true` |
| 28 | `exclusive;` `priority:` `rollback:` `retry` `timeout` | SAME with all three headers dropped in a single intent; see 2.4 | headers not compiled | headers: unverified (their meaning is conflict between intents, not tested); `retry`/`timeout`/`backoff`: dead surface |
| 29 | `with effects` | SAME with the clause dropped; declared `remote` while body derives `nondeterministic` rejected | not compiled | declared upper bound, like `caps` |
| 30 | `effect`, `object slot`, `refresh … from` | SAME vs struct + function (`5`) | not compiled | behavior replaceable for one read; source-bound refresh across mutation untested |
| 31 | `public`, `private` | SAME | same | unverified: one file has no module boundary to cross |
| 33 | operator operands | DIFF between C and LLVM (2.2) | C matches native C | not a word; a missing rule |
| 34 | bare field write in `func`/`action` | SAME (`90`) for `self.bal`, `bal` and `action` | same | no difference between the three spellings |
| 35 | `subject` parameter aliasing | `90`; `inout` on a subject fails in emitted C; `class`/`struct` parameters copy (`100`) | `100`; `inout` runs | the identity axis exists only on the native path (2.1) |

## 4. Verdict by registered word (146)

The registry (`src/lexer/language_keyword_registry.def`) has 70 reserved, 73
contextual and 3 soft words. Every row is listed once. "Case" points at the
experiment that carries the evidence; rows without a case are unverified by
this matrix and inherit nothing from being listed near a verified word.

| Verdict | Words | Case |
| --- | --- | --- |
| spelling removable, same fact retained in the tested shape | `loop`; one of `move`/`transfer`; two of `guard`/`expect`/`post`; `where` when `using` is present; local `mut` | 27, 09, 12, 08, 05 |
| behavior replaceable in one sequential pair; unsafe effect/parallel rejection differs | `unsafe` | 26 and the follow-up parallel control |
| dead surface (parser or checker refuses every use) | `timeout`, `backoff`, `retry` | 28 |
| declared upper bound, inferable when omitted | `caps`, `effects`, `nondeterministic`, `remote` (the two effect names executed) | 15, 29 |
| no same-guarantee replacement found | `match`, `case`, `default`; `extern`; `as`; `authority`; `dyn`, `bind` (open dispatch) | 04, 23, 24, 17, 22 |
| behavior replaceable, guarantee or cost differs (keep) | `defer`; `inout`; `intent`, `step`, `compensate`, `success`, `failure`; `spawn`, `await`, `async`; `parallel`, `join`, `give`; `ability`, `role`, `impl`, `requires`; `slot`, `own`, `ref`; `action`, `within`, `authorized`, `by`, `who`, `using` | 01, 02, 07, 13, 14, 21, 25, 06 |
| behavior replaceable, no separate guarantee observed in this shape | `class`, `vessel`, `party`, `roster`, `shared`, `event`, `object`, `effect`, `refresh`, `from`, `for` (range form), `zone` (one slot, one method) | 11, 20, 19, 18, 10, 30, 03, 16 |
| distinct phase, keep separate from the post-checks | `pre`, `invariant` | 12 |
| fold selector values, not concepts | `sum`, `product`, `min`, `max`, `all`, `any` | 14 (`sum`, `max` executed) |
| unverified by this matrix | `world`, `subject` (declaration itself), `tobject`, `relation`, `layer`, `state`, `lifecycle`, `activate`, `deactivate`, `maintain`, `detach`, `link`, `unlink`, `apply` (beyond 17), `map`, `to`, `subjects`, `objects`, `tobjects`, `relations`, `publish`, `projection`, `binding`, `involves`, `after`, `on`, `causes`, `select`, `every`, `continuous`, `blocking`, `concurrent`, `exclusive`, `priority`, `rollback`, `current`, `full`, `none`, `fail`, `transaction`, `pin`, `pool`, `capacity`, `forbids`, `with` (slot form), `secure`, `local`, `collapse` (never written), `between`, `namespace`, `import`, `use`, `export`, `public`, `private`, `type`, `is` (beyond scalar), `fields`, `innate`, `include`, `extends`, `override`, `reflect`, `func`, `let`, `if`, `else`, `while`, `break`, `continue`, `return`, `in`, `true`, `false`, `struct`, `enum` | 31 only for visibility |

Counting the rows against the registry: 70 words carry a verdict from an
executed pair and 76 do not (146 in total, no word in two rows). The
unverified row includes the base vocabulary (`func`, `let`, `if`, …) on
purpose: the matrix did not attempt to delete it, and a word is not
"confirmed necessary" by omission.

## 5. What this matrix does not establish

- Runtime equivalence on the LLVM backend for anything except `33_eval_order`.
- Conflict semantics of `exclusive`/`concurrent`/`priority`/`rollback`: one
  intent at a time cannot show them.
- Module boundaries for `public`/`private`/`namespace`/`import`/`export`.
- Multi-handler `event`, `causes`, and the world/layer/relation topology
  words: no pair was written.
- Whether the public path's 34 disagreements are one seam or many; the
  README's `source_admission_parity.sh` names eight of them as open claims.
- Any deletion. No word was removed and no checker rule was weakened.

## 6. Re-running

```sh
python3 tests/concept_semantics/word_deletion/run_matrix.py
PGY_EXTRA_FLAGS=--native-pipeline PGY_RESULTS=native.json \
    python3 tests/concept_semantics/word_deletion/run_matrix.py
```

Both commands need `bin/pgy` and, for the first, the installed self-host
driver. They are manual entry points, not CI jobs. Exit zero means collection
finished, not that the recorded outcomes reproduced: the runner has no baseline
comparison. Inspect compile/run outcomes separately, reject timeout or missing
artifact observations as verification failures, and compare against the recorded
results before claiming reproduction. It does not mean a word is safe to delete.

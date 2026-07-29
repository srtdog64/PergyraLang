# 138. Standard Library Scope

> 이 문서는 **무엇을**(scope ledger — 모듈 후보/tier/AI%)이고,
> **어떻게**(층/경계/7계약/inventory 게이트)는 `docs/148_stdlib_architecture.md`가
> 소유한다. 모듈의 현재 상태(active/sketch)는 148 §4 inventory 표가
> 정본이며 `stdlib-inventory-test-smoke`가 트리와의 일치를 잠근다. And Tiers

Last updated: 2026-06-22

Status: `planning / scope-contract`

The full list of what a Pergyra standard library needs, tiered and bounded. This
is a scope contract, not a to-do dump: it says what to build, what NOT to build,
and which part of each module is mechanical vs identity-defining.

## Principles (read first)

1. **Use-case-bounded, not Rust-complete.** Do not build "everything Rust/Go
   ship." Build what the killer use case ([[project_killer_usecase_dungeon_crawler]]:
   a deployable, capability-sandboxed interactive web app) actually hits. The use
   case is the fence; comprehensive stdlib is an open end and is out of scope.
2. **70% translation / 30% re-expression.** The algorithms (sort, hashmap, rope,
   B-tree) are solved problems with reference implementations in C/Rust/C++ — that
   70% is mechanical, AI-translatable, and verifiable against the reference test
   suite. The other ~30% is re-expressing each module onto Pergyra's distinctive
   model (ownership/slot, effect/capability, Result error discipline, domain
   axes). **That 30% is where the Pergyra stdlib is defined, not copied.**
3. **Semantic translation, not syntactic.** Porting Rust's `Vec` API verbatim
   imports Rust's borrow-checker mental model and makes the language feel like
   "Rust in a trenchcoat" (a marketing-drift path, [[feedback_marketing_language_drift]]).
   Port the *behaviour*; re-express the *ownership/effect/error* shape in Pergyra.
4. **Built post-self-host, in Pergyra, verified.** Stdlib is the validation
   milestone after self-host ([[project_no_self_host_decision]]): written in
   self-hosted Pergyra, each module checked against its reference behaviour and
   (where applicable) C/LLVM parity. AI does the 70%; the human owns the 30%.
5. **Signed-default numerics.** Surface exposes `Int`/`Long` only; no `UInt`/
   `USize` in stdlib signatures ([[project_signed_default_decision]]).
6. **Per-container combinators, no HKT.** `map`/`filter`/`reduce` are per-type
   functions, not a Functor/Monad abstraction ([[project_functor_hkt_stance]]).

## Already builtin (DO NOT rebuild)

These ship today as runtime builtins / language surface (evidence: hundreds of
`tests/cases/backend_compare/*` fixtures). The stdlib's job is to *formalize and
extend* them in Pergyra, not re-implement the basics:

- Collections: `Array<T>`, `List<T>`, `Set<T>`, `HashMap<String,T>`,
  `HashMap<Int,T>`, `Queue<T>`.
- Strings: concat, split, join, length, slice/substring, starts/ends-with,
  to-string, interpolation, parse-to-int.
- Numeric: arithmetic (checked div/overflow), abs/min/max, `Random` (cap-gated),
  `Now` (cap-gated), basic scalar math.
- IO: file read/write (cap-gated), dir-walk (cap-gated), `Log`, process args
  (cap-gated).
- Concurrency: `Channel<T>`, `parallel`, `spawn`/await, budget metering.
- Core types: `Option<T>`, `Result<T>`, `Vessel`, tuples, enums/tagged-unions.

## Tiers

- **P0** — the killer use case cannot ship without it.
- **P1** — common real-program need; expected of a 1.0 stdlib.
- **P2** — nice to have; add on demand.
- **P3** — explicitly out of scope until a concrete use case demands it (recorded
  so nobody silently grinds it).

## Module catalog

Columns: module — reference — status — Pergyra seam (the 30%) — AI-translatability
— tier.

### Collections
| module | ref | status | Pergyra seam | AI % | tier |
| --- | --- | --- | --- | --- | --- |
| dynamic array / list | Rust `Vec` | builtin | own/ref on elements; growth + slot interaction | 70 | P0 |
| hash map | Rust `HashMap` | builtin (String/Int keys) | generic key hashing/eq via `ability`; ownership of values | 60 | P0 |
| set | Rust `HashSet` | builtin | same as map | 70 | P0 |
| ordered/tree map | `BTreeMap` | TODO | comparator via `ability`; deterministic iteration | 70 | P1 |
| priority queue / heap | `BinaryHeap` | TODO | comparator `ability` | 80 | P1 |
| deque / ring buffer | `VecDeque` | partial (Queue) | bounded vs growable; channel overlap | 75 | P1 |
| stack | (Vec) | trivial over list | — | 95 | P1 |
| small/inline vector | `smallvec` | TODO | slot-lane allocation | 60 | P2 |
| linked list | `LinkedList` | — | rarely needed; own/ref chains are awkward | — | P3 |

### Sorting & selection (algorithms over collections)
| module | ref | status | Pergyra seam | AI % | tier |
| --- | --- | --- | --- | --- | --- |
| unstable sort | Rust `sort_unstable` (pdqsort) | TODO | comparator via `ability` (Ord); small-N base = sorting networks (AlphaDev-discovered) | 70 | P0 |
| stable sort | Timsort / driftsort | TODO | comparator `ability`; needs aux buffer (own/slot) | 60 | P1 |
| selection (nth/partial) | introselect | TODO | comparator `ability` | 75 | P2 |
| small fixed sort kernels | AlphaDev sort3/4/5 | TODO | branchless compare-exchange; ADOPT published networks, do not re-search | 80 | P0 |

**Optimal small-sort networks (PIN — transcribe these at stdlib time).** The
algorithm-level optimum for the small base cases is the classic minimal-comparator
sorting networks (Knuth TAOCP Vol 3; Batcher 1968), NOT something AlphaDev changed
(AlphaDev's win was the *assembly* lowering of these, which is the backend's job).
Pin them now so the stdlib's `sort_unstable` base case just transcribes them. A
comparator `CE(i, j)` = "if a[i] > a[j] (per the Ord `ability`), swap"; written as
a branchless compare-exchange so gcc/LLVM lower it without a branch.

```text
sort2 (1 comparator):   CE(0,1)
sort3 (3 comparators):  CE(0,2) CE(0,1) CE(1,2)
sort4 (5 comparators, depth 3):
    layer 1: CE(0,1) CE(2,3)
    layer 2: CE(0,2) CE(1,3)
    layer 3: CE(1,2)
sort5 (9 comparators):  CE(0,1) CE(3,4) CE(2,4) CE(2,3) CE(1,4)
                        CE(0,3) CE(0,2) CE(1,3) CE(1,2)
```

Use these for n <= 5 as the recursion base of the unstable sort; for 6..~16 either
extend with the known optimal networks or fall back to insertion sort (the usual
pdqsort threshold). Verify each transcription against an exhaustive 0/1-principle
test (a sorting network that sorts all 2^n binary inputs sorts all inputs) plus a
permutation test — this is the cheap, total correctness check for the pinned
networks. Parity: networks are deterministic, so C/LLVM outputs are bit-identical.

**Lower bounds (why these counts, and what AlphaDev did NOT change).** Two
*different* optima apply, and conflating them is a common error:

| n | adaptive lower bound `ceil(log2(n!))` | oblivious network min size | network min depth |
| --- | --- | --- | --- |
| 2 | 1 | 1 | 1 |
| 3 | 3 | 3 | 3 |
| 4 | 5 | 5 | 3 |
| 5 | 7 | **9** | 5 |
| 6 | 10 | 12 | 5 |
| 7 | 13 | 16 | 6 |
| 8 | 16 | 19 | 6 |

- **Adaptive bound `ceil(log2(n!))`** = the information-theoretic minimum number of
  comparisons for *any* comparison sort whose next comparison may depend on prior
  results (a decision tree; e.g. insertion sort). For n=3 this is 3: two
  comparisons distinguish only 4 outcomes < 3! = 6, so **sort3 in 2 comparisons is
  impossible.** This is a proof, not an engineering limit.
- **Oblivious network min size** = the minimum number of comparators for a *fixed,
  data-independent* sequence (a sorting network — what we pin, because it is
  branchless and CPU-friendly). It equals the adaptive bound for n<=4 but is
  strictly larger from n=5 (9 vs 7): the price of being branchless is extra
  comparators. The pinned `sort5` uses 9 — that is network-optimal, proven (Knuth
  TAOCP Vol 3; size optimality of n<=8 networks is established).

**What AlphaDev (Nature 2023) actually did**: it did **not** reduce either count —
both are proven optimal. Its sort3 win was the "swap move": eliminating one
redundant `mov` (data-movement) *instruction* in the x86 assembly lowering, while
keeping the 3 comparisons. That is an **assembly-level / instruction-selection**
optimisation, *below* the source level. A high-level language **cannot pin** it in
source; whether gcc/LLVM finds the same `mov` elision is the backend's job. So do
not document AlphaDev as "removing a comparison" — it removed an instruction.

**AlphaDev application (adopt / delegate / don't).** (1) ADOPT the optimal
comparator **networks** above at Pergyra source level (the algorithm-level optimum
— already pinned). (2) DELEGATE the assembly instruction win (AlphaDev's `mov`
elision) to gcc/LLVM; it is below source and not ours to control — its absence is
a backend-optimiser limit, not a stdlib defect. (3) do NOT re-run an AlphaDev-style
RL search (DeepMind-scale, out of scope). Parity: the network is deterministic, so
C/LLVM output is bit-identical; any per-backend instruction win is a *measurement*,
not a source contract. (Meta, future capability only, no overclaim: Pergyra's
parity gate could be the correctness oracle and the budget meter the cost function
for such a search over stdlib kernels.)

### Strings & text
| module | ref | status | Pergyra seam | AI % | tier |
| --- | --- | --- | --- | --- | --- |
| core string ops | Rust `str`/`String` | builtin | UTF-8 owned vs borrowed | 70 | P0 |
| string view (no-alloc) | `&str` | beta stdlib + fused builtins | `StrView` borrow over owned buffer; lifetime fact still to promote | 55 | P0 |
| builder / rope | `String`+/rope | partial | fused no-alloc builders already 11x; rope is the next rung | 50 | P1 |
| formatting | `format!` | partial (interpolation) | type-directed; effect-free | 70 | P1 |
| parsing (num/bool) | `str::parse` | partial (int) | Result-returning, no throw | 85 | P1 |
| unicode (graphemes/normalize) | `unicode-segmentation` | — | hard; scope to what the app renders | 60 | P2 |
| regex | `regex` | — | big; effect-free engine | 70 | P3 |

### Numeric & math
| module | ref | status | Pergyra seam | AI % | tier |
| --- | --- | --- | --- | --- | --- |
| scalar math (sqrt/pow/trig/log) | libm / `f64` | partial | effect-free; deterministic across backends (parity) | 90 | P1 |
| integer utils (gcd/clamp/...) | num crates | partial | signed-default | 95 | P1 |
| fixed-point / decimal | `rust_decimal` | — | for money/settlement (CLAUDE.md finance domain); exact, no float | 60 | P2 |
| big integer | `num-bigint` | — | own/ref on heap digits | 75 | P3 |

### IO & filesystem
| module | ref | status | Pergyra seam | AI % | tier |
| --- | --- | --- | --- | --- | --- |
| buffered reader/writer | `BufReader` | TODO | capability-gated (IO_READ/WRITE); Result | 70 | P1 |
| path manipulation | `std::path` | TODO | pure string ops, effect-free | 85 | P1 |
| stdin/stdout/stderr | `std::io` | partial (Log) | cap-gated; line buffering | 80 | P1 |
| file open-handle API | `File` | partial (read/write builtins) | handle lifecycle = slot/Vessel; cap-gated | 50 | P1 |

### Time
| module | ref | status | Pergyra seam | AI % | tier |
| --- | --- | --- | --- | --- | --- |
| instant / now | `Instant` | builtin (`Now`, cap CLOCK) | cap-gated; nondeterministic effect | 80 | P1 |
| duration arithmetic | `Duration` | TODO | pure value type | 95 | P1 |
| date/calendar format/parse | `chrono` | — | effect-free; large | 75 | P2 |

### Encoding & serialization
| module | ref | status | Pergyra seam | AI % | tier |
| --- | --- | --- | --- | --- | --- |
| JSON encode/decode | `serde_json` | TODO | **P0 for the web/sandbox use case** (save/load, wire); Result; effect-free | 70 | P0 |
| base64 / hex | crates | TODO | pure, effect-free | 95 | P1 |
| UTF-8 validate | builtin-ish | partial | already mostly handled | 90 | P1 |
| CSV | crate | — | pure | 90 | P2 |
| binary (de)serialize | `bincode` | — | layout-stable; ABI overlap | 60 | P2 |

### Iterators & functional combinators
| module | ref | status | Pergyra seam | AI % | tier |
| --- | --- | --- | --- | --- | --- |
| per-container map/filter/reduce/fold | Rust `Iterator` | partial | **per-type, NOT HKT**; effect-aware (closures may carry effects) | 50 | P0 |
| range / enumerate / zip / take / drop | `Iterator` adapters | partial | lazy vs eager decision; per-type | 60 | P1 |

### Error / Option / Result utilities
| module | ref | status | Pergyra seam | AI % | tier |
| --- | --- | --- | --- | --- | --- |
| Option/Result combinators (map/andThen/unwrapOr) | Rust | partial: value-level done | predicates/unwrap/UnwrapOr are builtins; `stdlib/option.pgy` (2026-07-03) adds per-type bridges OptionOr/OkOption/OptionToResult (Int/String) with a backend-compare fixture. map/andThen deliberately deferred: callable params await docs/141 Stage B + F1 carrier. Generic `<T>` form blocked: C codegen does not monomorphize generic functions over Option<T> (probed; TODO board) | 70 | P0 |
| typed error vocabulary | `thiserror` | — | structured `AppError`-style (CLAUDE.md §1.2); stage/code | 50 | P1 |

### Concurrency (mostly builtin — formalize, don't rebuild)
| module | ref | status | Pergyra seam | AI % | tier |
| --- | --- | --- | --- | --- | --- |
| channels | builtin | builtin | session-type direction (future, docs/19) | — | P0 |
| mutex / atomic | `std::sync` | runtime | world/zone isolation usually replaces shared mutation | 60 | P1 |
| thread pool / scheduler | builtin | builtin | budget-metered (R6) | — | P1 |

### Pergyra-distinctive (the identity 30% — NOT in any other stdlib)
These have NO C/Rust reference to translate; they are where Pergyra's stdlib is
*designed*, and they are the reason the stdlib is not "Rust in a trenchcoat":
| module | what it is | tier |
| --- | --- | --- |
| zone/world helpers | spawning, message-passing, isolation utilities over `world`/`zone` | P1 |
| intent helpers | building/composing intent step graphs, compensation utilities | P2 |
| capability/effect utilities | manifest building, `with caps` helpers, sandbox grant sets | P1 |
| slot/lifecycle helpers | `host_task_slot`: keyed generation authority for host task publication/cleanup; typed resource lifecycles (Vessel state machines) as reusable patterns | active/P1 |
| ability/witness library | common abilities (Ord, Eq, Hash, Show) as the dispatch substrate collections use | P0 |

### Use-case (dungeon crawler / sandbox) specifics
| module | ref | status | tier |
| --- | --- | --- | --- |
| 2D grid / tile map | — | TODO | P0 |
| seeded RNG | builtin (`Random`/`SeedRandom`) | builtin | P0 |
| save/load (JSON) | serde | TODO (see JSON P0) | P0 |
| pathfinding (A*) | — | likely app-level, not stdlib | P2 |

## The 30% checklist (every module must answer)

When porting any module, the mechanical 70% is the algorithm; the 30% that makes
it Pergyra is answering these for the module's API:

1. **Ownership**: who owns elements/buffers — `own` vs `ref`? How does it interact
   with `slot`/`Vessel` lifecycle? (Not Rust's borrow checker — Pergyra's model.)
2. **Effect/capability**: does any operation touch ambient authority (IO, clock,
   random)? Then it carries an `effect`/`capability` the caller must hold.
3. **Error**: failures return `Result` with a typed code — never throw
   (CLAUDE.md §1.2). No silent fallback.
4. **Determinism/parity**: same input → same output across C and LLVM (and future
   backends). Floats and iteration order must be backend-stable.
5. **Domain axes**: does it interact with `zone`/`world`/`intent`? If so, that
   interaction is the distinctive part — design it, don't borrow it.

## Build order (AI-leveraged)

1. **P0 ability library + per-container combinators** (the dispatch substrate
   everything else uses) — design-heavy, human-led.
2. **P0 JSON + StrView + grid + Option/Result combinators** — the killer use case
   unblockers; mostly translation, AI-led with the 30% review.
3. **P1 collections (tree map, heap, deque), strings (builder, format, parse),
   math, IO (buffered, path), time, encoding (base64/hex)** — parallel,
   AI-translatable, each verified against its reference suite.
4. **P1 Pergyra-distinctive helpers** (zone/capability/slot/intent) — design-heavy,
   human-led; this is the stdlib's identity.
5. **P2 on demand. P3 only when a concrete use case appears.**

## References & cross-check anchors

For each module, the canonical paper/standard (cross-check the *algorithm*) and a
reference implementation (cross-check the *API/behaviour*). The 70% you translate
against these; the 30% (the Pergyra seam) has no external reference — that is the
point. A citation here is an algorithm/spec anchor, not a license to copy the
ownership/effect shape.

### Collections
- **Hash map** — Open-addressing + Swiss-tables layout: Matt Kulukundis, *Designing
  a Fast, Efficient, Cache-friendly Hash Table, Step by Step*, CppCon 2017 (abseil
  `flat_hash_map`; Rust `hashbrown`). Robin Hood probing: Celis, Larson, Munro,
  *Robin Hood Hashing*, FOCS 1985. Default hasher / DoS-resistance: Aumasson &
  Bernstein, *SipHash: a fast short-input PRF*, INDOCRYPT 2012. Refs: Rust
  `std::collections::HashMap`, abseil, C++ `unordered_map`.
- **Ordered / tree map** — B-trees: Bayer & McCreight, *Organization and
  Maintenance of Large Ordered Indexes*, Acta Informatica 1972. Cache-friendly
  node sizing: Bender, Demaine, Farach-Colton, *Cache-Oblivious B-Trees*, FOCS
  2000. Red-black (alt): Guibas & Sedgewick, *A Dichromatic Framework for Balanced
  Trees*, FOCS 1978. Refs: Rust `BTreeMap`, C++ `std::map`.
- **Priority queue / heap** — Williams, *Algorithm 232: Heapsort*, CACM 1964
  (binary heap). Fibonacci (usually overkill): Fredman & Tarjan, *Fibonacci heaps
  ...*, JACM 1987. Refs: Rust `BinaryHeap`, C++ `priority_queue`.
- **Dynamic array growth** — amortized doubling: Cormen, Leiserson, Rivest, Stein,
  *Introduction to Algorithms* (CLRS), ch. on amortized analysis. Growth-factor
  (1.5x realloc-in-place): Folly `fbvector`. Refs: Rust `Vec`, C++ `vector`.
- **Deque / ring buffer** — Refs: Rust `VecDeque`, C++ `deque`.

### Sorting & selection
- **Hybrid unstable sort** — Musser, *Introspective Sorting and Selection
  Algorithms*, Software: P&E 1997 (introsort); Orson Peters, *Pattern-defeating
  Quicksort* (pdqsort), 2014/2021 (Rust `sort_unstable`).
- **Stable sort** — Tim Peters' Timsort (CPython/Java/Rust); worst-case bound:
  Auger et al., *On the Worst-Case Complexity of TimSort*, ESA 2018. Modern: Orson
  Peters' driftsort/glidesort (Rust 1.81 stable sort).
- **Sorting networks (small-N base case)** — Knuth, *TAOCP Vol 3: Sorting and
  Searching*; Batcher, *Sorting Networks and their Applications*, AFIPS 1968.
  ML-discovered shorter kernels: Mankowitz, Michi, Zhernov, Gelmi, et al.,
  *Faster sorting algorithms discovered using deep reinforcement learning*
  (AlphaDev), Nature 618:257–263, 2023 — merged into LLVM libc++ `std::sort`;
  **adopt the discovered networks, do not re-search** (see catalog note).

### Strings & text
- **Rope** — Boehm, Atkinson, Plass, *Ropes: an Alternative to Strings*, Software:
  Practice and Experience 1995.
- **Substring search** — two-way (glibc/Rust default): Crochemore & Perrin,
  *Two-way string-matching*, JACM 1991. KMP: Knuth, Morris, Pratt, *Fast Pattern
  Matching in Strings*, SIAM J. Comp. 1977. Boyer-Moore, CACM 1977.
- **UTF-8 / Unicode** — RFC 3629 (UTF-8); The Unicode Standard; UAX #29 (grapheme/
  word segmentation); UAX #15 (NFC/NFD normalization).
- **Float <-> string** — to-string: Adams, *Ryū: fast float-to-string conversion*,
  PLDI 2018. string-to-float: Lemire, *Number Parsing at a Gigabyte per Second*,
  Software: P&E 2021 (Rust uses the Eisel-Lemire fast path). Cross-check parity:
  both backends must agree bit-for-bit.
- **Small-string optimization** — Refs: libc++/libstdc++ `std::string` SSO.

### Numeric & math
- **Floating point** — IEEE 754-2019. Math functions: libm reference. Determinism
  caution: cross-backend float parity requires fixed rounding + no fast-math.
- **Decimal / fixed-point (money)** — Cowlishaw (IBM), *General Decimal Arithmetic
  Specification* (IEEE 754-2008 decimal); Fowler, *PoEAA* Money pattern. Aligns
  with CLAUDE.md finance domain (exact, no float for settlement).
- **Big integer** — Knuth, *TAOCP Vol 2: Seminumerical Algorithms*; ref GMP.
- **PRNG** — non-crypto: O'Neill, *PCG ...*, 2014; Blackman & Vigna, *Scrambled
  Linear PRNGs* (xoshiro/xoroshiro), ACM TOMS 2021. crypto (keep separate): DJB,
  *ChaCha20*. The stdlib MUST distinguish non-crypto (reproducible, seedable —
  what games want) from CSPRNG (security). `Random` is currently cap-gated RANDOM.

### Encoding & serialization
- **JSON** — RFC 8259 + ECMA-404. API model: serde (Rust). (P0 for the web/sandbox
  use case.)
- **Base64 / Hex** — RFC 4648.
- **CSV** — RFC 4180.
- **Binary** — refs: bincode, FlatBuffers, Cap'n Proto (overlaps the Pergyra ABI,
  docs/04 — cross-check against the existing layout owner before inventing one).

### Time
- **Format** — ISO 8601; RFC 3339 (internet timestamps).
- **Civil-time algorithms** — Howard Hinnant, *chrono-Compatible Low-Level Date
  Algorithms* (days<->civil date). Timezones: IANA TZ database (P3).

### Iterators & functional combinators
- **Iterator/adapters** — GoF Iterator; Rust `Iterator` (lazy adapters). Pergyra
  decision: per-container, NOT HKT ([[project_functor_hkt_stance]]); cross-check
  the *non-HKT* choice against: Wadler & Blott, *How to make ad-hoc polymorphism
  less ad hoc*, POPL 1989 (typeclasses/dictionaries — what abilities reify) and
  against transducers (Hickey, Clojure) as the alternative composition model.

### Error / Option / Result
- **Combinators** — Rust `Option`/`Result`; Haskell `Maybe`/`Either`. Composition:
  Wlaschin, *Railway-Oriented Programming*. Typed error: CLAUDE.md §1.2 `AppError`
  (stage/code), not exceptions. Cross-check that no combinator hides control flow
  (CLAUDE.md §1.1).

### Pergyra-distinctive (NO external reference to translate)
- **ability / witness library** — the dispatch substrate; the reference is the
  *theory*, not a stdlib: Wadler & Blott (POPL 1989) typeclasses + dictionary
  passing. See [[project_witness_evidence_passage]] and docs/semantics/10.
- **zone/world, intent, capability/effect, slot helpers** — no C/Rust analog by
  design. Cross-check against the *language theory* in docs/semantics/19 (ambient
  calculus, effect systems, authorization logic, typestate), not against another
  stdlib. This is the identity layer.

### Use-case (dungeon crawler)
- **A\* pathfinding** — Hart, Nilsson, Raphael, *A Formal Basis for the Heuristic
  Determination of Minimum Cost Paths*, IEEE T-SSC 1968 (likely app-level, not
  stdlib). **Grid / tilemap** — standard; cross-check against Pergyra's
  `Array`/slot model, not an external lib.

## Honest boundary

This list is bounded by the use case, so it has an end (unlike "all of Rust").
The ~70% is AI-translatable from existing references and verifiable against their
test suites; the ~30% (ownership/effect/error/domain re-expression + the
Pergyra-distinctive modules) is the human-owned work that defines what the Pergyra
standard library *is*. Do not let the 70% (easy translation) crowd out the 30%
(the identity), and do not let "be comprehensive" reopen the P3 fence.

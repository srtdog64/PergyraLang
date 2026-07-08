# 170. Keyword, Lineage, And CS Application Map

Status: `routing-map`, design documentation (2026-07-06)

This document is not a new source of truth for syntax. It is a routing map for
contributors: when a keyword, programming style, external language influence, or
computer-science topic is mentioned, this page says where it belongs in
Pergyra's implementation and documentation.

Read this together with:

- `119_pergyra_lineage_positioning.md` for lineage and non-overclaiming.
- `124_syntax_pattern_matrix.md` for syntax adoption decisions.
- `125_source_of_truth_spine.md` for compiler fact ownership.
- `168_fortran_parallel_evidence.md` for data-parallel language evidence.
- `169_agent_boundary_sentinel_library.md` for repository guardrails for
  LLM/agent-authored code.

## 1. Rule

Do not copy a language feature because a successful language has it. Translate
the underlying pressure into a Pergyra owner:

```text
external pattern -> Pergyra keyword/fact -> CS basis -> implementation owner
```

If there is no owner, the feature is not ready. Parser acceptance alone is not
implementation.

### Game-World Extraction Note

Pergyra's domain vocabulary was extracted first from games, TRPGs, board-game
rules, and fiction because those domains have spent decades under one unusually
clear pressure: describe a world efficiently enough that humans and machines can
act inside it. This is not arbitrary sampling. The same vocabulary cluster is
also reached independently by BDI agent theory (`intent`, agent, role),
Searle-style institutional/context rules, UFO/OntoUML role and phase
ontologies, and Kripke/ML5-style possible-world language. That convergence is
the triangulation evidence; if the game-derived heuristic were arbitrary, this
independent convergence would be hard to explain.

## 2. Keyword And Method Map

| Pergyra surface | Main method | Influenced by | CS basis | Apply in |
|---|---|---|---|---|
| `func`, hosted method | ordinary callable abstraction | C#, C, ML-family | lambda calculus, type checking, call ABI | parser, semantic, DAG/type, MIR routine, C/LLVM emit, self-hosted parser/semantic |
| `let`, `let mut`, assignment | local binding and controlled mutation | ML, Rust, Swift, C# | scope rules, SSA, dataflow | semantic locals, CFG body dataflow, MIR local facts, C/LLVM local emission |
| `if`, `while`, `for`, `break`, `continue` | structured control flow | ALGOL/C-family, Rust | CFG, dominance, reachability, cleanup dataflow | parser statement owners, CFG verifier, MIR block lowering, backend parity |
| `match`, enum cases | closed branching over values | ML/OCaml/F#/Rust/C# | ADTs, exhaustiveness, pattern matching | semantic pattern owner, CFG facts, MIR branch facts, diagnostics |
| `struct`, `class` | passive data and nominal behavior | C, C#, Swift | records, nominal type systems, ABI layout | DAG/type owner, ABI/layout rows, C/LLVM struct emit |
| `subject`, `object`, `tobject` | active domain identity, read model, transport shape | C# DDD practice, CQRS/DTO patterns | domain modeling, projection, type-as-medium | semantic domain owners, projection tests, ABI layout, docs/121 |
| `ability`, `role` | domain contract and binding | C# interface, Rust trait, typeclass family | bounded polymorphism, dictionary/vtable dispatch | DAG/type resolution, MIR declaration inventory, ability-ref metadata, C/LLVM dispatch |
| `party`, `roster`, `relation` | participation and domain graph composition | DDD, actor/relationship modeling | graph theory, relation algebra | MIR declaration inventory, domain projection, self-hosted architecture docs |
| `zone`, `world` | resource/trust/failure boundary | actor systems, capability systems, Erlang/Verona pressure | capability security, isolation, state machines | AIR boundary evidence, runtime materialization facts, diagnostics, self-hosted CompilerWorld |
| `intent` | observable orchestration spine | workflow/saga libraries, DDD use cases | state machines, effect systems, graph planning | parser/domain syntax, AIR intent evidence, compression/erasure gates, LSP diagnostics |
| `effect`, `authority`, `authorized by` | side-effect and permission evidence | Koka/effect systems, capability security | effect systems, proof obligations, access control | semantic authority checks, AIR evidence, runtime boundary policy |
| `Slot<T>`, `SecureSlot<T>`, `pin`, views | resource handles and scoped access | Rust borrow/pin ideas, Vale generational handles | ownership, alias control, linear capability pressure | slot runtime, semantic pin checks, ABI ownership, evidence-driven guard amortization |
| `ref`, `own`, `inout` | boundary-passing modes | Rust, Swift, C++ references | aliasing, value-result semantics, ownership transfer | semantic parameter modes, ABI ownership shape, diagnostics |
| `async`, `await`, `spawn`, `parallel`, `Channel<T>`, `select` | structured execution and communication | C#, Erlang, Go, Rust async | concurrency theory, CSP, scheduling, happens-before | SEA/lane facts, memory-concurrency model, C/LLVM async/parallel lowering |
| `parallel on`, future bulk loops | data-parallel projection | Fortran, APL, MATLAB, Julia | independence proofs, reductions, vectorization | `168_fortran_parallel_evidence.md`, CFG read/write sets, ABI layout, future target projection |
| `Option<T>`, `Result<T,E>`, `?`, `??` | explicit absence/failure flow | Rust, Swift, ML-family | sum types, totality, failure contracts | type checker, diagnostics, self-hosted semantic/LSP, C/LLVM parity |
| `namespace`, `import`, `export` | scalable code organization | C#, TypeScript, Python packages | module systems, symbol tables | resolver, package/module owner, self-hosted path owners |
| `extern`, `unsafe`, raw boundary | explicit escape to host/system layer | C, Ada/SPARK, Rust/Zig contrast | ABI contracts, capability boundaries, undefined-behavior control | ABI docs, raw-escape contract, runtime-none profiles, backend emit |
| `bits(...)`, `reinterpret(...)` direction | explicit bit conversion vs boundary crossing | Zig/C/C# layout pressure | representation theory, endianness, ABI layout | `145_bit_layout_boundary_matrix.md`, ABI/layout owner, backend fail-closed gates |
| lambdas, pipeline, interpolation, tuples | ergonomic expression composition | C#, F#, Python/JS ergonomics | lambda calculus, product types, string languages | surface hygiene, parser/semantic owners, self-hosted pain-point closure |

## 3. Influenced-Language Map

| Language/family | What to take | What not to take | Apply in Pergyra |
|---|---|---|---|
| C# | broad industrial shape, async surface, generics, pattern matching, tooling feel | managed-runtime assumptions as the substrate | syntax ergonomics, LSP/tooling, self-hosted authoring shape |
| C | ABI, AOT portability, FFI, predictable native output | macro-driven hidden semantics, unstructured UB | C backend, runtime ABI, extern/raw boundary |
| Rust | fail-closed ownership pressure, Result/Option, MIR discipline | lifetime annotations as the user-facing model | Slot/pin/CFG/MIR verifiers, diagnostics, backend fail-closed gates |
| Swift | value/reference distinction, `inout` clarity, ergonomic Option/Result pressure | ARC as the core memory model | parameter-mode wording, value-result docs, surface ergonomics |
| Fortran/APL/MATLAB/Julia | explicit bulk array intent, independence, reductions, layout facts | implicit alias trust, global layout destiny | data-parallel evidence facts and future CPU/GPU/tensor/NPU projection |
| Erlang/Go | message passing, process/channel intuition, failure isolation | untyped actor/channel surface as the whole model | world/zone boundary, Channel, select, runtime materialization policy |
| OCaml/F#/Haskell/ML | ADTs, pattern matching, type inference discipline | HKT/functor hierarchy as core beta surface | enum/match/Option/Result, type checker, diagnostics |
| Koka/effect languages | effects as typed evidence | effect labels detached from authority/resource boundaries | intent/effect/authority AIR facts |
| D | compile-speed discipline, order-independent declarations, pragmatic systems lessons | CTFE/mixins/metaprogramming as core | compiler speed docs, declaration inventory discipline |
| Zig | explicit layout and allocation questions, comptime contrast | hidden logical bit-order defaults, comptime as the core thesis | ABI/layout docs, explicit `bits`/`reinterpret` boundary |
| Ada/SPARK/F*/Dafny/Lean/Rocq/Agda | proof obligations and verifier culture | claiming model proof as implementation proof | formal docs, IR verifiers, proof-carrying pipeline gates |
| SQL/DOP/DDD practice | separation of read/write views, aggregates, domain relations | ORM-style hidden runtime magic | subject/object/tobject, relation, projection, intent |

## 4. Computer-Science Topic Map

| CS topic | Pergyra usage | Apply next |
|---|---|---|
| Formal languages and automata | lexer/parser/token grammar, reserved syntax, diagnostics | grammar golden tests, parser/lexer diagnostic gates, self-hosted parser parity |
| Type theory | generics, abilities, Option/Result, domain coordinates | DAG/type resolver, ability default resolution, generic-axis composition gates |
| Operational semantics | meaning of statements, cleanup, panic/failure, value-result modes | formal semantics docs, MIR verifier, runtime panic contract |
| CFG and dataflow | branch/loop safety, reassignment, cleanup, capture analysis | `cfg-body-dataflow` gates, MIR source-local fact consumption |
| SSA and compiler IR | local redefinition, branch joins, MIR backend parity | MIR block lowering, C/LLVM unified consumption, backend fail-closed gates |
| Graph theory | AST/DAG/MIR/AIR graphs, world/zone/relation topology | AIR graph validators, declaration inventory, self-hosted artifact graph tools |
| Information theory and abstraction loss | compression, projection, loss contracts, erasure vs materialization | AIR loss metrics, runtime materialization evidence, target projection docs |
| Complexity and optimization | compile speed, deterministic iteration, canonical output | compiler-speed engineering, stable maps/sets, deterministic artifact ordering |
| Parallel computation | execution lanes, channels, worker boundaries, data-parallel independence | SEA lane facts, Fortran-derived DP facts, worker-boundary UB gates |
| Security and cryptography | tokens, authority, secure slots, audit logs | security-portability gates, structured security logs, capability boundaries |
| ABI and representation theory | layout, niche, explicit bit order, extern/raw escape | ABI golden tests, bit-layout matrix, runtime-none contracts |
| Proof engineering | model checking, proof obligations, typed evidence | Coq/Lean/Rocq models only after implementation facts exist; IR verifiers first |
| Human-computer interaction | diagnostics with reason/fix, LSP squiggles, concise surface | diagnostic registry, LSP parity, user pain-point docs |

## 5. Advantages To Import, Costs To Reject

The right unit is not "language X has feature Y". The right unit is "this
advantage solves a Pergyra problem without importing the source language's
failure mode."

| Source pressure | Advantage worth importing | Cost not to import | Pergyra translation |
|---|---|---|---|
| C# industrial surface | readable multi-paradigm code, async familiarity, strong tooling expectations | GC/JIT/reflection as hidden runtime dependencies | keep the broad feel, but lower through explicit ABI/runtime facts |
| C systems substrate | transparent ABI, simple AOT output, easy host integration | macro preprocessor semantics, unchecked aliasing, silent UB | C is a projection target and FFI boundary, not the semantic owner |
| Rust safety pressure | fail-closed ownership discipline, Result/Option culture, MIR-grade compiler facts | lifetime syntax as the user's business-modeling surface | Slot/pin/CFG facts carry safety; users see domain boundaries and diagnostics |
| Swift value semantics | clear value/reference split, `inout` naming for value-result mutation | ARC as the core memory strategy, implicit copy-cost surprises | document value-result modes honestly and keep ownership evidence explicit |
| Fortran data parallelism | no-alias pressure, bulk array intent, reductions, layout-aware lowering | implicit trust in compiler alias assumptions, global layout defaults | derive visible data-parallel facts before CPU/GPU/NPU projection |
| APL/MATLAB/Julia array culture | concise whole-array thinking and mathematical projection | shape errors hidden behind dynamic runtime behavior | use explicit shape/layout facts and diagnostics before sugar |
| Erlang actor model | failure isolation, message passing, let-it-crash boundary thinking | making everything an actor/process | use `world`/`zone` boundaries only where resource/failure ownership exists |
| Go concurrency | simple channel mental model and select-style coordination | untyped goroutine sprawl and hidden shared-memory races | channel/result handoff plus worker-boundary UB gates |
| ML/OCaml/F#/Haskell | ADTs, pattern matching, totality pressure, type inference discipline | HKT/functor abstraction tower as core language requirement | keep ADT/match/Option/Result; defer higher-kinded abstractions to libraries |
| Koka/effect systems | effects as first-class evidence | effect labels detached from authority and resources | bind effects to `intent`, `authority`, `zone`, and AIR evidence |
| D compiler engineering | compile-speed discipline and pragmatic native tooling | CTFE/mixins/template metaprogramming as core semantics | copy the engineering discipline, not the metaprogramming surface |
| Zig explicitness | allocation/layout questions asked at the surface | comptime as the thesis, hidden logical bit-order defaults | explicit `bits` and `reinterpret` boundaries with ABI facts |
| Ada/SPARK/F*/Dafny/Lean/Rocq/Agda | proof obligations, contracts, and mechanized models | replacing implementation closure with a separate proof model | prove rules after the implementation owner and gate exist |
| SQL/DOP/CQRS/DDD | read/write model separation, aggregates, relations, projections | ORM-style hidden runtime magic and stringly queries | first-class `subject`/`object`/`tobject`/`relation`/`intent` facts |
| Python/JavaScript ergonomics | fast authoring, interpolation, simple scripting feel | dynamic type drift and late runtime discovery of semantic errors | add ergonomic sugar only when static owners and diagnostics remain explicit |
| TypeScript structural surface | scalable application vocabulary and tooling ecosystem pressure | structural typing as unchecked shape coincidence | prefer named domain facts and explicit projection rows |

## 6. Computer-Science Advantages And Traps

| CS area | Advantage worth importing | Trap not to import | Pergyra application |
|---|---|---|---|
| Type theory | precise ADTs, generics, constraints, and total failure surfaces | abstraction for its own sake | abilities, Option/Result, generic-axis gates |
| Category theory | composition vocabulary and lawful transformation intuition | making user syntax depend on category jargon | intent/effect/projection composition should be visible but practical |
| Automata/formal languages | lexer/parser determinism and explainable syntax errors | grammar cleverness that users cannot predict | grammar golden tests and clear diagnostics |
| Graph theory | explicit topology for worlds, zones, relations, DAGs, AIR/MIR | graph objects that become second semantic stores | graph facts are projections of owners, not hidden truth |
| Information theory | measured abstraction loss and compression/erasure accounting | claiming zero-cost when runtime materialization remains | AIR loss metrics and materialization evidence |
| Complexity theory | canonical forms, deterministic ordering, predictable build cost | optimizing before semantics are closed | stable iteration, canonical output, compile-speed gates |
| Parallel computation | independence, reductions, scheduling lanes, hardware projection | treating lane choice as vectorization proof | separate SEA lane facts from data-parallel facts |
| Cryptography/security | capability tokens, structured audit, fail-closed boundaries | custom crypto or free-text security events | standard primitives and structured security logs |
| Formal proof | machine-checked confidence for stable rules | proof model used as a substitute for implementation | IR verifiers first, proof after fact ownership |
| HCI/tooling | diagnostics that teach Reason/Fix and editor feedback | clever errors that hide the real owner | diagnostic registry, LSP squiggles, pain-point closure |

## 7. Where To Apply Next

Near-term application should stay implementation-owned, not slogan-owned:

1. **Self-hosted compiler**: use `intent` and `zone` to organize compiler
   resources, but keep facts owned by parser/semantic/AIR/MIR/ABI modules.
2. **Data-parallel evidence**: derive `ArrayLayoutFact`,
   `IterationIndependenceFact`, and `ReductionFact` before claiming Fortran
   class optimization or NPU/tensor projection.
3. **Backend parity**: remove backend guesses. Missing type/layout/authority
   facts must fail closed or be added to MIR/AIR/ABI owners.
4. **User ergonomics**: improve Option/Result consumption, tuple returns,
   interpolation, and diagnostics without adding aliases for existing concepts.
5. **Agent/repo guardrails**: keep `169_agent_boundary_sentinel_library.md`
   separate from language semantics. It is for future code authors, not for
   Pergyra users.
6. **Formal proof work**: prove classification rules only after the
   implementation owner exists. A proof model must not be used to pretend a
   missing implementation is closed.

## 8. Negative Rule

If a new feature pitch cannot fill this row, defer it:

```text
keyword/surface:
influenced-by:
CS basis:
source-of-truth owner:
first positive fixture:
first negative fixture:
backend parity gate:
docs to update:
```

This is the practical filter that keeps Pergyra from becoming a list of copied
language features. The feature must become a Pergyra fact path.

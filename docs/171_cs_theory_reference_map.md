# 171. CS Theory Reference Map

Status: `reference-map`, design documentation (2026-07-06)

This document maps computer-science theory to concrete Pergyra implementation
owners. It is not a proof that Pergyra already implements every theory listed
here, and it is not a license to add academic vocabulary to the user surface.

The rule is:

```text
theory -> invariant or evidence -> owner -> gate
```

If a theory cannot name an invariant, owner, and gate, it is background reading,
not an implementation requirement.

## 1. Reading Order

| Priority | Topic | Why it matters to Pergyra | Reference |
|---|---|---|---|
| 1 | Programming language foundations | Gives the baseline vocabulary for operational semantics, Hoare logic, and static type systems. | [Software Foundations](https://softwarefoundations.cis.upenn.edu/) |
| 2 | Type systems | Grounds `Option`, `Result`, generics, abilities, and typed compiler evidence. | [Types and Programming Languages](https://www.cis.upenn.edu/~bcpierce/tapl/) |
| 3 | SSA and compiler IR | Explains why reassignment, branch joins, and phi-like facts must be explicit in MIR/LLVM lowering. | [LLVM Language Reference](https://llvm.org/docs/LangRef.html), [Cornell CS 6120 SSA lesson](https://www.cs.cornell.edu/courses/cs6120/2022sp/lesson/6/) |
| 4 | Discrete math | Provides the shared foundation for sets, relations, graphs, partial orders, state machines, induction, and counting. | [MIT 6.042J Mathematics for Computer Science](https://ocw.mit.edu/courses/6-042j-mathematics-for-computer-science-fall-2010/) |
| 5 | Algorithms and data structures | Grounds deterministic collections, graph passes, asymptotic cost, and compiler-scale data structures. | [MIT 6.006 Introduction to Algorithms](https://ocw.mit.edu/courses/6-006-introduction-to-algorithms-spring-2020/) |
| 6 | Model checking and state machines | Useful for world/zone/slot lifecycle, concurrency, cancellation, and rollback models. | [TLA+ Hyperbook](https://lamport.azurewebsites.net/tla/hyperbook.html) |
| 7 | Effect systems | Directly relevant to `intent`, `effect`, `authority`, and AIR evidence. | [Koka](https://koka-lang.github.io/), [Microsoft Research Koka](https://www.microsoft.com/en-us/research/project/koka/) |
| 8 | Information theory | Gives language for abstraction loss, compression, projection, and retained runtime materialization. | [Shannon, A Mathematical Theory of Communication](https://people.math.harvard.edu/~ctm/home/text/others/shannon/entropy/entropy.pdf) |
| 9 | Proof assistants | Good for later soundness models, but only after implementation facts exist. | [Rocq Books](https://rocq-prover.org/books), [Lean Language Reference](https://lean-lang.org/doc/reference/latest/) |

## 2. Theory To Pergyra Owner Map

| Theory area | Use in Pergyra | Owner | Gate direction | Reference |
|---|---|---|---|---|
| Computability | Treat Turing completeness as the floor, not the design goal; abstraction-loss contracts describe what the program means beyond mere computability. | language philosophy, semantics docs | abstraction-loss contract smoke | [Church-Turing Thesis](https://plato.stanford.edu/entries/church-turing/) |
| Lambda calculus | Function abstraction, application, lambdas, callable values, and substitution discipline. | parser/function owners, type checker, callable ABI | lambda/codegen parity and capture diagnostics | [Lambda Calculus](https://plato.stanford.edu/entries/lambda-calculus/) |
| Formal languages and automata | Tokenization, grammar, parse recovery, reserved syntax, and deterministic diagnostics. | lexer/parser owners | parser/lexer diagnostic smoke, grammar golden tests | [MIT 6.042J](https://ocw.mit.edu/courses/6-042j-mathematics-for-computer-science-fall-2010/) |
| Type systems | `Option`, `Result`, generics, ability bounds, payload typing, and fail-closed semantic facts. | DAG/type resolver, semantic checker | type-resolution and semantic parity gates | [TAPL](https://www.cis.upenn.edu/~bcpierce/tapl/) |
| Operational semantics | Statement meaning, environment changes, cleanup, panic, value-result mutation, and branch behavior. | formal semantics docs, MIR verifier | MIR/body verifier and runtime panic gates | [Software Foundations](https://softwarefoundations.cis.upenn.edu/) |
| Hoare logic | Preconditions, postconditions, loop invariants, and state-transformer reasoning for slots/effects. | CFG/body dataflow, formal proof notes | cfg-body-dataflow smoke and proof-carrying pipeline gates | [Software Foundations](https://softwarefoundations.cis.upenn.edu/) |
| SSA | Reassignment becomes new definitions; joins need explicit phi-like facts instead of AST guessing. | MIR block lowering, LLVM backend | backend fail-closed and LLVM parity | [LLVM LangRef](https://llvm.org/docs/LangRef.html), [Cornell SSA](https://www.cs.cornell.edu/courses/cs6120/2022sp/lesson/6/) |
| Graph theory | AST/DAG/MIR/AIR traversal, reachability, dominance, declaration inventories, relation topology. | graph validators, declaration inventory owners | AIR graph, MIR declaration inventory, source inventory gates | [MIT 6.042J readings](https://ocw.mit.edu/courses/6-042j-mathematics-for-computer-science-fall-2010/pages/readings/) |
| State machines | Slot lifecycle, pin/view state, cancellation, select/await progress, rollback. | runtime/semantic slot owners, SEA/lane owners | slot contract, memory-concurrency, runtime ABI lifetime gates | [TLA+ Hyperbook](https://lamport.azurewebsites.net/tla/hyperbook.html) |
| Effect systems | `intent` should derive effect, authority, materialization, and coordination evidence. | AIR evidence, semantic authority owner | AIR drift, runtime authority, intent-compression gates | [Koka](https://koka-lang.github.io/) |
| Information theory | Measure what is compressed, erased, summarized, or retained across AIR/MIR/backend projection. | AIR abstraction-loss and materialization owners | air-erasure/materialization gates | [Shannon 1948](https://people.math.harvard.edu/~ctm/home/text/others/shannon/entropy/entropy.pdf) |
| Algorithms and complexity | Deterministic iteration order, stable emission, build speed, graph pass cost, and canonicalization. | compiler data-structure owners, build/perf docs | stage4 determinism, perf contract, compile-speed gates | [MIT 6.006](https://ocw.mit.edu/courses/6-006-introduction-to-algorithms-spring-2020/) |
| Parallel computation | Execution lanes, worker boundaries, data-parallel independence, reductions, and scheduling constraints. | SEA/lane owner, Fortran-derived DP evidence owner | worker-boundary UB, data-parallel evidence, backend parity gates | [MIT 6.006](https://ocw.mit.edu/courses/6-006-introduction-to-algorithms-spring-2020/), [TLA+ Learning](https://lamport.azurewebsites.net/tla/learning.html) |
| Proof engineering | Mechanize selected soundness rules only after owner facts and fixtures exist. | proof-carrying pipeline docs, Rocq/Lean experiments | proof-carrying adequacy and formal-semantics gates | [Rocq Books](https://rocq-prover.org/books), [Lean](https://lean-lang.org/) |
| Homotopy/dependent type theory | Long-range reference for equality, identity, and proof-carrying design; not a beta surface goal. | future proof-model notes only | no implementation claim until a concrete owner exists | [The HoTT Book](https://homotopytypetheory.org/book/) |

## 3. Theory Import Rules

### 3.1 Computability Is Not Enough

Turing completeness says what can be computed in principle. Pergyra's design
target is narrower and more useful: what can be carried through intent,
authority, ownership, effect, projection, and materialization evidence without
guessing.

Apply to:

- `semantics/09_abstraction_loss_contracts.md`
- AIR compression/materialization gates
- target-neutral projection docs

Do not apply as:

- a claim that `if`/`while`/`for` are enough for the language thesis;
- a reason to ignore loss, authority, layout, or runtime evidence.

### 3.2 Type Theory Becomes Owner Facts

Type theory should not become surface ornament. It should become facts:

- payload type of `Option<T>` / `Result<T,E>`;
- actual/default generic resolution;
- ability-ref formal/actual/default mapping;
- ABI layout row for type representation;
- diagnostic facts when a type decision fails.

The owner must be DAG/type metadata or MIR/ABI facts, not a backend guess.

### 3.3 Operational Semantics Becomes MIR Verifier Work

For Pergyra, operational semantics is not just a document. It should force
questions like:

- What state changes when `inout` copy-out happens?
- What happens on panic or early return?
- Which cleanup edges run through branch, loop, `break`, `continue`, and
  `return`?
- Which slot/pin state transitions are legal?

Apply this to MIR and CFG verifiers before optimizer work.

### 3.4 Effect Theory Must Stay Bound To Authority

Effect systems are useful only if the evidence is connected to resources and
authority. A freestanding effect label is not enough for Pergyra.

Required path:

```text
intent -> effect -> authority evidence -> zone/world boundary -> AIR/MIR fact
```

Do not add a second authority channel if `effect` can own the requirement and
runtime evidence can prove the boundary.

### 3.5 Graph Theory Must Not Create A Second Store

Graphs are useful for traversal, reachability, dependency, and topology. They
become dangerous when the graph copy is treated as a second source of truth.

Rule:

```text
graph node = projection of owner fact
```

The graph may explain, validate, and compare. It must not recover semantic
truth by reparsing text or AST when a typed owner exists.

### 3.6 Information Theory Frames Abstraction Loss

Pergyra uses abstraction layers intentionally. The question is not "is there
loss?" but "what loss is declared, bounded, and verified?"

Apply to:

- AIR loss metrics;
- erasure/materialization decisions;
- emitted C/LLVM/SelfHosted artifact parity;
- future CPU/GPU/NPU projection records.

### 3.7 Proof Assistants Are Late-Stage Tools

Rocq/Lean/TLA+ can model rules. They do not close implementation debt by
themselves.

Use them for:

- state-machine validation;
- small soundness lemmas;
- proof-carrying pipeline checks;
- regression prevention once a rule is already implemented.

Do not use them for:

- claiming a missing verifier is implemented;
- replacing C/LLVM/self-hosted parity;
- hiding fallback paths behind a model.

## 4. First Application Targets

| Target | Theory to apply | Concrete next owner |
|---|---|---|
| Self-hosted semantic checker | Type systems, operational semantics, diagnostics | shared diagnostic/semantic fact owners |
| MIR branch and loop facts | SSA, CFG/dataflow, Hoare-style invariants | MIR verifier and backend fail-closed gates |
| Intent/effect/authority | Effect systems, state machines | AIR evidence and runtime authority contract |
| Slot/pin lifecycle | State machines, capability security | slot contract and runtime ABI lifetime gates |
| Data-parallel projection | Discrete math, algorithms, parallel computation | `168_fortran_parallel_evidence.md` DP fact ladder |
| Abstraction loss metrics | Information theory | AIR erasure/materialization measurement |
| Canonical output | Algorithms, complexity, graph theory | deterministic collection and artifact ordering owners |
| Proof-carrying pipeline | Proof engineering | formal semantics and proof-carrying adequacy gates |

## 5. Link Hygiene

Prefer stable sources in this order:

1. official course/book/project pages;
2. original papers or publisher pages;
3. university-hosted lecture notes;
4. encyclopedia pages only for orientation;
5. blogs/forums only when they explain an implementation pattern not covered by
   a primary reference.

When a source influences implementation, record the Pergyra owner and gate in
this file or the more specific design document. A raw link is not a design
decision.

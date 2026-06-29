# Staged Roadmap

Self-hosting is a staged validation path, not a single rewrite.

## North Star (committed terminus)

The final destination of this track is the **self-eating bootstrap**: the
Pergyra-written compiler compiles its own source into a working compiler that
reproduces itself — a 3-stage fixed point (see Stage 5). Stages 0-4 are not the
goal; they are the staged, parity-guarded path that makes the fixed point
reachable without an unverified fork. Partial self-host (Stage 4) is the
committed *interim* posture; the self-eating bootstrap is the committed *end
state*. It is gated behind Beta closure and the typed-AST migration, and it is a
substrate milestone rather than the language thesis (the thesis is proven by the
domain primitives in real programs, not by self-compilation) — but it remains
the declared terminus of the self-host track. BDFL, 2026-06-29.

## Stage 0 - Beta Closure

Exit criteria:

- CFG/MIR body safety is the source of truth for stable body checks.
- AIR validates abstraction boundaries with strict evidence.
- DAG type resolution has no retired recursive resolver usage or unresolved
  metadata dead-end on stable paths.
- MIR declaration inventory drives C/LLVM hosted-method identity.
- ABI ownership and Slot/Pin contracts are frozen.
- Dogfood WebGL bridge is proven through C backend output, not core language surface.

No hard substitution rung is promoted before this stage exits and the matching
oracle gate is green.

## Stage 1 - Soft Self-Host

Write compiler-adjacent tools in Pergyra.

Target tools:

- Diagnostic catalog checker.
- AIR graph JSON validator.
- MIR dump diff tool.
- C/LLVM backend output comparator.
- Module/package resolver helper.

Input should be JSON, text dumps, or manifest files. Avoid linking directly
against compiler internals.

## Stage 2 - Partial Self-Host

Move isolated validation passes or transform helpers into Pergyra once Stage 1
tools are useful.

Candidate areas:

- Diagnostic registry validation.
- AIR evidence consistency checks.
- MIR declaration inventory consistency checks.
- Module manifest normalization.
- Stable subset conformance checks.

Each partial pass must run beside the C implementation before replacing it.

## Stage 3 - Core Migration Prototype

Only after partial self-host succeeds:

- Choose one bounded compiler owner.
- Implement the Pergyra version.
- Run both implementations.
- Compare outputs.
- Keep rollback to C implementation trivial.

Parser/type checker/backend migration is still not automatic at this stage.

## Stage 4 - Hard Self-Host

Hard self-host is active as staged substitution, not as a full compiler fork.
A rung is allowed only when:

- generated artifacts are deterministic,
- tooling can debug Pergyra-written compiler code,
- module/package resolver is stable,
- FFI and scoped unsafe raw-escape contracts are stable; plain `unsafe { ... }`
  must not grant raw/system-tier escape,
- C compiler parity remains available as a reference,
- LLVM parity remains available as the second oracle where enabled,
- SoT closure is a pass condition: missing MIR/AIR/DAG/ABI facts are fixed at
  the owner or rejected with structured diagnostics, not hidden behind a
  compatibility fallback.

## Stage 5 - Self-Eating Bootstrap (the terminus)

The committed end state. The compiler stops being something the C compiler
builds and becomes something that builds itself.

The fixed point (classic 3-stage bootstrap):

- stage1: the C compiler compiles the self-host source into compiler binary A.
- stage2: A compiles the self-host source into compiler binary B.
- stage3: B compiles the self-host source into compiler binary C.
- B and C must be byte-identical. That equality is the proof of self-hosting.

Entry is gated; this stage is not attemptable until all of the following hold:

- Stage 0 (Beta closure) has exited.
- Parser/codegen completeness: the self-host front-to-back can parse and lower
  every construct its own ~18k-LOC source uses (the Stage 4 rungs close this
  one construct at a time; e.g. closures, generics, `match`, `Option`,
  `Array<class>`, nested arrays must all round-trip).
- A fixed-point harness exists and gates B == C; a divergence is a hard failure,
  not a tolerated drift.
- C and LLVM parity remain available as references throughout (the bootstrap is
  proven against the dual oracle, never trusted blind).
- Recommended-not-required: the typed-AST migration has retired the text->text
  munging core (tracked by the Pergyra-likeness ratchet,
  `tests/self_host_pergyra_likeness_smoke.sh`). A text-munging compiler can
  technically reach the fixed point, but the BDFL preference is to bootstrap an
  idiomatic compiler, not an un-Pergyra one. Likeness may instead be ratcheted
  up after the fixed point if the symbolic milestone is pulled forward.

Honest scope: reaching this terminus is a substrate achievement (every serious
language eventually self-hosts) and does not by itself advance the language
thesis. It is recorded here as the declared destination, not as the highest-
leverage near-term work. Sequencing against Beta closure and the dungeon-crawler
dogfood is a separate priority call.

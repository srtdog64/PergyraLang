# Staged Roadmap

Self-hosting is a staged validation path, not a single rewrite.

## Stage 0 - Beta Closure

Exit criteria:

- CFG/MIR body safety is the source of truth for stable body checks.
- AIR validates abstraction boundaries with strict evidence.
- DAG type resolution has no retired resolver or materializer fallback usage on stable paths.
- MIR declaration inventory drives C/LLVM hosted-method identity.
- ABI ownership and Slot/Pin contracts are frozen.
- Dogfood WebGL bridge is proven through C backend output, not core language surface.

No self-host migration starts before this stage exits.

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

Hard self-host is allowed only when:

- generated artifacts are deterministic,
- tooling can debug Pergyra-written compiler code,
- module/package resolver is stable,
- FFI and raw escape contracts are stable,
- C compiler parity remains available as a reference.


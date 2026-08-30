# Binary Reconstruction Resistance Target

Status: `APPROVED RELEASE TARGET — ACCEPTANCE GATE OPEN — NOT AN ACTIVE IMPLEMENTATION RUNG`

Updated: 2026-08-30 (Asia/Seoul)

This document owns the release-artifact reconstruction-resistance target. It
does not claim that current binaries meet the target, and it does not make
obfuscation a language semantic or a source-of-truth owner.

This is an approved product goal, not a proposal. `OPEN` refers only to the
missing acceptance evidence: until the cross-backend comparison gate closes,
the project must report C++-class reconstruction resistance as unproven.

## Target

A shipped Pergyra `--opt=release` executable must be no easier to reconstruct
into source-level structure than a conventional optimized and stripped C++
release built for the same target with the same host toolchain.

`C++-class` means all of the following:

- a decompiler may recover approximate control flow, data flow, constants, and
  externally observable behavior, as it can for C++;
- comments, source formatting, local names, module paths, and exact Pergyra
  `world` / `zone` / `subject` / `action` / `intent` source structure are not
  recoverable from release-only metadata;
- public FFI/ABI names and user-visible string literals may remain when the
  program contract requires them;
- no claim is made that native code is impossible to reverse engineer.

The comparison baseline is a small equivalent C++ corpus compiled with the
same compiler, target, release optimization, LTO policy, and the platform's
normal debug/symbol stripping. Pergyra must not exceed the baseline in any
individual leakage class; a low aggregate score cannot excuse one severe
source-path or metadata leak.

## Objective card

- Objective: keep the primary release executable at C++-class reconstruction
  resistance while preserving a useful explicit developer-debug path.
- Priority order: runtime and ABI correctness; one C/LLVM release policy;
  removal of source/debug metadata; non-public symbol minimization; debug
  sidecar usability; size and optimization.
- Policy owner: this release contract until one implementation lease names a
  code owner at the final artifact boundary. C and LLVM may not grow separate
  release-security policy.
- Last legitimate consumer: the final platform link/strip/package boundary
  that publishes the executable and an optional separately retained debug
  sidecar.
- Forbidden fallback: treating `-O3` as proof, leaving debug information and
  relying on users to strip it, backend-specific behavior, stripping required
  public ABI symbols, hiding a leak with a weighted score, or counting packers
  and anti-debug tricks as language correctness.
- Verification gate: a cross-backend release-artifact gate compares Pergyra
  and C++ baseline leakage classes, executes both Pergyra binaries for behavior
  parity, and negatively mutates each forbidden leak.

## Required release properties

The primary C- and LLVM-backed release executables must satisfy the same
policy:

1. No DWARF, CodeView, embedded PDB path, line table, or local-variable debug
   record remains in the shipped executable. Debug builds or explicit debug
   sidecars may retain them.
2. No absolute workspace path, source root, `.pgy` input path, temporary build
   path, or host username remains.
3. Non-exported source routine, method, binder, and type spellings do not
   survive merely for diagnostics or convenience. Required public ABI exports
   use an explicit allowlist and stable ABI owner.
4. HIR/MIR/AIR JSON, SoT registry names, compiler-internal diagnostic details,
   source excerpts, and Pergyra-native evidence schemas are absent unless an
   explicit runtime-observability contract owns their presence.
5. Release optimization, internalization, dead-code elimination, visibility,
   and LTO decisions are shared policy facts rather than ad-hoc backend flags.
6. Removing metadata never changes stdout/stderr, exit state, panic class,
   authority checks, ownership behavior, or the public FFI ABI.
7. An explicit developer profile retains actionable source lines and symbols,
   preferably in a separate debug artifact bound to the exact executable
   identity.

String literals, protocol field names, public export names, and reflection or
observability data explicitly required by the program remain visible. This is
the same limitation conventional C++ releases have; encrypting values needed
by a running local process is not a general secrecy proof.

## Acceptance evidence

The target may move to `CLOSED` only when one executable gate covers both C and
LLVM release paths on supported platforms and observes all of these:

- primary executable debug-section and embedded-debug-path absence;
- workspace/source path absence;
- seeded private source-identifier absence with explicit public-export
  allowlisting;
- high-level IR/evidence-schema residue absence;
- release runtime parity before and after artifact hygiene;
- developer debug artifact/source-line usability;
- a same-toolchain C++ baseline report for each leakage class;
- negative fixtures proving that injected debug, path, private-symbol, and IR
  residues make the gate fail.

Decompiler output is useful audit evidence but is not the sole automated gate:
decompiler heuristics and versions drift. Static artifact properties and
behavioral parity are the stable acceptance boundary.

## Current observation

The 2026-08-30 probe is evidence of an open target, not closure:

- the public driver defaults to `--opt=release`, and C/LLVM host compilation
  selects `-O3`;
- public `examples/hello.pgy --emit-llvm --opt=release` contains `main`, the
  required `printf` declaration, and string literals, but not the source-level
  `Main` spelling or Pergyra domain structure;
- public C- and LLVM-backed release executables did not contain the
  `hello.pgy` path in the observed probe;
- both executables still contained debug sections contributed by the linked
  toolchain/runtime, readable `pgy_*` runtime symbols remain, and the compiler
  owns no final artifact strip policy;
- the native libLLVM path can create full DIBuilder line/function metadata
  when a MIR source path is present, while the public self-host LLVM route has
  a different emission shape. The future policy must close that path split.

Therefore current Pergyra is optimized and loses substantial source shape,
but C++-class release reconstruction resistance is not yet proven. Opening
this target does not change the SoT census, strict beta percentage, integrated
project forecast, or current self-host substitution progress.

## Explicit non-goals

- malware-style evasion, self-modifying code, anti-debugging, or VM detection;
- DRM, packer, or encrypted-loader requirements;
- a promise that secrets embedded in a client executable remain secret;
- hiding documented public FFI/runtime ABI names;
- changing Pergyra semantics to make reverse engineering inconvenient.

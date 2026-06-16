# Self-Host Groundwork Readiness

Operational status of the prerequisites in `02_required_language_surface.md`,
checked against the actual codebase, with the concrete next pass to start the
middle-end self-host. This is the groundwork map: it pins what is ready, what is
empty, and what the first real middle/back pass needs, so that pass starts
without inventing dead substrate.

## Readiness matrix

Each row is evidence-backed, not aspirational.

Language substrate (the axis that gates whether a compiler pass can be written
in Pergyra at all):

- Collections: ready. `Array` (push/pop/length/map/filter/sort/reverse/set),
  `List`, `Set` (add/has/remove/size/values), `Queue`, and `Map`
  (`MapNew/Get/Set/Has/Keys/Remove/Size`) are all present as builtins. Map is
  string-keyed, which covers symbol tables and name-to-decl indices directly.
- Allocation lanes: present (allocator surface exists; effect system models
  `alloc` lanes).
- Scoped unsafe/raw escape: present (scoped unsafe capability blocks).
- Generics with ability bounds: present and monomorphized on both backends.
- `Result<T, E>` and explicit failure: present.
- FFI / `extern "C"` and C backend: present.
- IR dump schema: present (AIR JSON graph dump, MIR dump/validation).
- Tool surface: `FileExists`, `Exit`, `Args`, `ReadFile`, JSON emit/parse used
  by the live self-hosted tools.

Conclusion: the language surface meets the hard-self-host prerequisites. The
blocker to hard self-host is not missing syntax or missing collections; it is
that the middle/back passes are unwritten.

Self-hosted component state:

- Lexer: done, byte-identical to `pgy --tokens` on both backends.
- Parser: done, byte-identical to `pgy --ast` on the committed fixtures, both
  backends, including intent retry metadata.
- Semantic: rung-2, a bounded function-body subset (typed let/return, operators,
  calls, arity/arg checks, branch-condition Bool, scoped blocks), 30 parity
  fixtures, both backends. Not yet a full semantic pass.
- HIR / MIR / AIR / codegen / compiler: no compiler-core substitution yet. The
  component directories hold only a README. Five AIR graph consumer tools live
  under `src/self_hosted/tools/` as soft evidence, not as `src/self_hosted/air/`
  replacement.
- Shared lib (`self_hosted/lib/`): one module, `text_scan.pgy`. No compiler data
  structures yet (symbol table, worklist, graph) -- correctly, because nothing
  consumes them yet.

## What "finish the groundwork" actually means

Two things, in order, and neither is "add a language feature":

1. Pick the smallest real middle pass to self-host first, and stand up its
   parity harness against the C oracle, mirroring the existing tool-parity
   pattern (compile the Pergyra pass on C and LLVM, run it beside the C pass,
   assert byte-identical output on a fixture set). The substrate is ready; the
   missing thing is the first dog-fed pass plus its gate.

2. Grow `self_hosted/lib/` only as that pass needs it. The first pass will need
   a small, deterministic set of structures -- almost certainly an ordered
   symbol/name index (on `Map`) and a worklist (on `Queue` + a `Set` for
   dedup). Write each lib module when its first consumer lands, never before, so
   it is never unread code.

## First middle-consumer slice

Start at the seam where a Pergyra pass can read a stable dumped IR and emit a
stable checkable result, so the parity gate is a pure text diff -- the same
shape that made the diagnostic-catalog and AIR-graph-validator tools tractable.
The first slice is now live as five rung-1 AIR graph consumers:

- `air_graph_id_uniqueness`: no duplicate node ids.
- `air_graph_node_count_integrity`: live AIR dump `"id":` count matches the
  compiler-owned summary counts.
- `air_graph_ref_live`: live AIR dump `boundary` and `intent` back-references
  stay within summary-count bounds.
- `air_graph_ref_integrity`: every edge endpoint points at an existing node id.
- `air_graph_reachability`: every node is reachable from a declared root via a
  push-only worklist.

These are still soft consumers, not compiler-core substitution. One already
use the drift-guarded live AIR dump fixture; the next promotion is to feed
reachability and id uniqueness from live `pgy --air-json` output and compare
against C-owned summaries. A producer pass (Pergyra emitting HIR/MIR/C) is the
larger second step and should follow only after at least one live-dump consumer
is green on both backends.

## Sequence

1. Promote the remaining AIR graph consumers from fixture-shaped JSON to live
   `pgy --air-json` input.
2. Add the C-owned summary or oracle counts that the Pergyra consumers must
   match.
3. Keep the C/LLVM leg in `make self-host-preparation-test-smoke`.
4. Add the one or two lib structures it needs (worklist, name index) under
   `self_hosted/lib/`, each with its first consumer present.
5. Only after a live-dump consumer pass is green do producer passes (HIR/MIR
   emission in Pergyra) begin, lowest IR first.

## What is explicitly out of scope for groundwork

- Adding language features: not needed, the surface is ready.
- Writing lib data structures speculatively: forbidden until a consumer exists.
- Producer passes before a consumer pass is green: sequenced later.
- Going below LLVM (own native backend): out of scope permanently; LLVM and the
  C backend stay the two targets, C as the parity oracle.

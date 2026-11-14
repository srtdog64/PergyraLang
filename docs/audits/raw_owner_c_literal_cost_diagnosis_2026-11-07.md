# C literal initialization cost: bounded diagnosis

Observed 2026-11-07 KST. This is a generated-output experiment, not a
production compiler change or installed-driver result. The user questioned
the previously measured 3-4x overhead and prioritized the default C route.

## Finding

The fixed constant `Array<Int>` literals are emitted as a capacity-sized
allocation followed by one `pgy_array_push_Int` call per element. Clang 18
`-O2` leaves those calls in the final C-route executable: Dijkstra and A*
each execute 833 initialization calls per Work; Union-find executes 512.
The C route already reserves the literal length, so **its problem is not
reallocation**. The original LLVM route additionally grows from zero capacity.

Replacing only the constant literal construction in a scratch generated C
file with the same checked allocation, a constant memory copy and final length
publication removes the repeated push calls. Array access checks and the
compiler-generated drops are unchanged. No additional source annotations,
manual drops, unchecked access or alias promises were introduced.

At the same flags, input, binary controls and CPU, the longer 120,000-Work
confirmation measured these medians over seven interleaved samples:

| Algorithm | Direct C | Original generated C | Scratch literal change | Original/direct | Changed/direct |
|---|---:|---:|---:|---:|---:|
| Dijkstra | 139.454 ms | 408.898 ms | 147.025 ms | 2.93x | 1.05x |
| A* | 136.095 ms | 385.638 ms | 149.462 ms | 2.83x | 1.10x |

The six-algorithm shorter comparison measured 0.84-1.14x direct C after this
one change. That range is specific to these small fixed workloads. It is not
a general claim that Pergyra beats C or a forecast for arbitrary programs.
The [immutable run receipt](../truth/evidence/2026-11-07_raw_owner_c_literal_cost_diagnosis.md)
owns exact identities, all six measurements, validation and artifact paths.

## Owner and repair boundary

- Producer: the already typed constant Array literal and its element count.
- Current native C owner: `emit_array_literal_expression` in
  `src/codegen/c/transpiler_expr_composite_literal_emit.c`, especially the
  per-element push emission. Its frozen and current source hashes are equal.
- Native LLVM sibling: `src/codegen/llvm/llvm_expr_aggregate.c`; it zeroes the
  Array descriptor and emits one push per element without preallocation.
  Its frozen and current source hashes are also equal. No LLVM speedup was
  measured in this follow-up.
- Last consumers: initialized Array backing, existing checked indexing and
  synthesized retirement. Allocation failure remains owned by the existing
  Array allocator/runtime.
- Forbidden shortcut: erasing indexing or ownership checks, emitting alias
  promises without evidence, or applying scalar byte copying to owning values.

Before a production repair, map the corresponding self-host emitters and
existing literal initialization contract. Constant scalar bulk initialization
is a supported experiment result, not a license to change effectful element
evaluation order, partial initialization cleanup, String/nested/owning element
transfer, empty literals, custom allocators or failure behavior by analogy.
Preserve those obligations and compare installed C/LLVM behavior separately.

## Verified boundary

All six changed C executables match 251 independently computed seed answers
each. ASan/UBSan plus explicit allocation observations show all 13,581
allocations freed in the six 503-Work checks, with no live blocks. Separate
nonzero-literal read/write probes preserve 20 successful results and eight
intended bounds refusals, including negative and one-past-end indices.

The large slowdown in these workloads is removable code generation cost.
The remaining unknown is general production coverage, not whether this
particular 3x gap is an unavoidable ownership cost. Main's active V.001 lane,
compiler sources, safety policies and installed binaries were not changed.

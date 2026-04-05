**Intent**
`adapter_policy_stack` validates three practical language surfaces together:

- function-typed values used as real policy injection
- nested `HashMap<String, List<String>>` style collection composition
- `page / api / report` split as library-shaped adapters instead of domain execution boundaries

**Layout**
- `common.pgy`: route adapter class, API envelope tobject, formatter policies
- `pages/surfaces.pgy`: page object plus request builders/renderers
- `report/pipeline.pgy`: report assembly helpers over list buckets
- `main.pgy`: adapter-heavy composition entry

**What It Proves**
- `func(...) -> ...` values can live in locals, flow through returns, and be invoked on both C and LLVM
- nested generic type arguments parse and lower correctly in real code
- adapter code can stay outside `zone/world` while still producing structured output
- `page -> api -> report` layering is library-shaped, not a forced language primitive

**Current Notes**
- the example intentionally keeps nested collection usage in locals instead of exported function signatures on the C side
- LLVM parameter registration now tracks collection/function metadata for free functions and hosted methods, which was the latent bug exposed by this scenario

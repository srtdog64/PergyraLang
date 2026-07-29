# Stdlib Beta Freeze

Status: beta-freeze-source-of-truth.

This document defines the standard-library surface that can be treated as
beta-stable. Anything not listed here is experimental or out-of-beta even if a
file exists under `stdlib/` or a parser/compiler path recognizes the name.

Completion rule:

- A stable stdlib item must have syntax, semantic typing, C backend behavior,
  LLVM backend behavior, diagnostics, smoke coverage, and docs aligned.
- `make stdlib-test-smoke` is the executable gate for this document.
- The stable list must stay aligned with `type_checker_stdlib_use.c`.

## Stable Builtin Stdlib Surface

These are compiler/runtime builtins, not `use` modules:

- Logging and console: `Log`, `LogBlock`, `LogBanner`, `LogRaw`, `Print`,
  `ReadLine`.
- File IO: `FileOpen`, `FileRead`, `FileWrite`, `FileClose`, `ReadFile`,
  `WriteFile`, `FileExists`, `DirWalk`. `DirWalk(String) -> Array<String>`
  returns a deterministic, lexicographically sorted owned snapshot of regular
  files under the requested directory, using `/` path separators.
- Strings: `Concat`, `StringLength`, `Contains`, `StringIndexOf`, `Replace`,
  `Substring`, `Trim`, `Split`, `Join`, `Upper`, `Lower`, `ToString`.
- Numeric helpers: `Abs`, `Min`, `Max`.
- Time helpers: `Now`, `Sleep`.
- Process/tooling helpers: `Args`, `Exit`. `Args() -> Array<String>` returns
  the user arguments passed to the generated binary, excluding the executable
  name, as an owned snapshot.
- Allocation helpers: `AllocatorSystem`, `AllocatorPool`,
  `AllocatorDebug`, `AllocatorTracing`.
  `AllocatorScratch`, `AllocatorResult`, `AllocatorPersistent` are the stable
  lane-named constructors, and `AllocatorDestroy(allocator)` is the stable
  named-local cleanup operation for those lanes. These produce and consume the
  single stable `Allocator` value used by allocation-aware runtime owners; LLVM
  lowers them through pointer-initialized runtime exports to avoid
  platform-specific struct-return ABI drift. The scratch/result/persistent
  helpers carry distinct runtime lane kinds, not aliases of the same builtin
  flow. `BoxArray(capacity, allocator)` accepts a named `Allocator` local on C
  and LLVM so fused array storage keeps a stable owner.
- Collections frozen for beta: `Array<T>`, `Slice<T>`, `List<T>`, `Set<T>`,
  `Queue<T>`, `HashMap<String, T>`, `HashMap<Int, T>`, `HashMap<Long, T>`,
  `HashMap<Bool, T>`. `SliceCopy(Slice<T>) -> Array<T>` is the stable
  borrowed-view to owned-snapshot escape hatch. `MapKeys` returns a
  deterministic owned snapshot: strings lexicographic, integers and longs
  ascending, and booleans `false` before `true`. `SetValues` returns the same
  deterministic owned snapshot for `Set<String|Int|Long|Bool>`. Compiler-facing
  symbol, record, and handle keys must be normalized to these scalar key forms:
  canonical strings for symbol/record identities and stable integer/long IDs for
  handles. Raw aggregate keys are not part of the beta-stable collection
  contract.
- Result/Option baseline: `Ok`, `Err`, `IsOk`, `IsErr`, `Unwrap`, `UnwrapOr`,
  `Some`, `None`, `IsSome`, `IsNone`, `UnwrapOption`.

## Stable `use` Modules

These modules are beta-stable when imported through `use <module>;`:

- `datetime`: `LocalDate`, `LocalTime`, `DateTime`, `Duration`, `Instant`,
  `SameDate`, `FormatDate`, `FormatTime`, `FormatDateTime`,
  `DateTimeOnDate`, `DurationMs`, `DurationSeconds`, `DurationMinutes`,
  `FormatDuration`, `InstantNow`, `InstantAdd`, `DurationBetween`,
  `DeadlineReached`.
- `money`: `Money`, `MoneyOf`, `MoneyZero`, `MoneySameCurrency`, `MoneyAdd`,
  `MoneySub`, `MoneyNeg`, `MoneyEq`, `MoneyGte`, `RenderMoney`.
- `timer`: `TimerSpec`, `TimerAfter`, `TimerEvery`, `TimerExpired`,
  `TimerRemaining`, `TimerTick`, `RenderTimer`.
- `versioning`: `VersionStamp`, `IdempotencyKey`, `VersionInitial`,
  `VersionNext`, `SameVersion`, `MakeIdempotencyKey`,
  `SameIdempotencyKey`, `RenderVersion`, `RenderIdempotencyKey`.
- `ledger`: `LedgerEntry`, `LedgerPosting`, `DebitEntry`, `CreditEntry`,
  `BuildTransferPosting`, `LedgerBalanced`, `RenderLedgerEntry`,
  `RenderLedgerPosting`.
- `obligation`: `Obligation`, `Violation`, `ObligationCheck`,
  `OpenObligation`, `FulfillObligation`, `ObligationDue`,
  `EvaluateObligation`, `RenderObligation`, `RenderViolation`.
- `device_adapter`: `DeviceRegister`, `DeviceSample`, `DeviceCommand`,
  `Register`, `SampleDevice`, `WriteDevice`, `SampleEventTopic`,
  `RenderDeviceSample`, `RenderDeviceCommand`.
- `host_task_slot`: `HostTaskTicket`, `HostTaskSlot`,
  `HostTaskSlotTransition`, `HostTaskApplyPolicy`, `HostTaskPolicyDecision`,
  the typed `HostTasks.SpawnPolicy`, `HostTasks.RestartPolicy`,
  `HostTasks.SkipPolicy`, `HostTasks.ApplyPolicy` admission surface, and the
  `HostTasks.Open`, `HostTasks.Ticket`, `HostTasks.IsCurrent`,
  `HostTasks.Replace`, `HostTasks.PublishWait`, `HostTasks.PublishFinal`,
  `HostTasks.Cleanup`, `HostTasks.Phase` authority operations. Active skip and
  duplicate spawn preserve generation; restart advances it. Stale tickets
  cannot publish a wait/final outcome or authorize cleanup.

The stable module smoke covers these modules together because several domain-kit
modules intentionally depend on common modules such as `money` and
`versioning`.

## Known But Experimental Modules

These module names may be recognized by the compiler so examples and future
work do not fail at the `use` boundary, but they are not beta-stable:

- `http`: transport adapter draft.
- `storage`: persistence adapter draft.
- `page`: UI/page adapter draft.
- `spray`: GPU/Spray design placeholder.

Experimental modules must not be advertised as stable. Their APIs can change
without beta compatibility guarantees.

## Out Of Beta

- Package-manager distribution of stdlib modules.
- Version-resolution policy for third-party modules.
- Supply-chain integrity and signing.
- GPU/Spray runtime and shader/render integration.
- Skia/render graph adapters.
- Rich storage/page/http production adapters.
- Full FP/HKT/functor library surface.

## Regression Gate

`make stdlib-test-smoke` must:

- Run the stable builtin stdlib probe on the C backend.
- Run the same builtin probe on the LLVM backend when requested.
- Run the stable `use` module probe on the C backend.
- Run the same stable `use` module probe on the LLVM backend when requested.
- Check this document contains the stable/experimental module taxonomy.

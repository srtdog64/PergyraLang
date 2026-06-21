# 13. `PGY_RAW_SLOTS` - the raw/unsafe whole-program build mode

Status: **DESIGN.** The macro exists and is honoured at the runtime layer
(`pgy_runtime_panic_checked_inline.h`); the *driver flag* and *mix-safety
enforcement* below are proposed, to be implemented with end-to-end verification.

Companion to [[project_slot_safety_consistency]] (the always-on default decision).

## 0. What it is

Slot safety checks (generational use-after-release / double-release detection)
are **on by default in every build** - fail-closed, not zero-cost (see
`pgy_abi_spec.h` section 1). `PGY_RAW_SLOTS` is the **single, explicit opt-out**: a
whole-program raw/unsafe build that eliminates the `occupied` field and its
checks for systems-tier code where the cost has been *measured* and the static
own/ref + interprocedural release analysis is trusted to carry safety alone.

It is **not** the beta-stable canonical ABI. It is a deliberate, named departure,
the way `unsafe` is in other languages - visible, opt-in, never the default.

## 1. The hard constraint: never mix raw and checked

`PGY_RAW_SLOTS` changes the `Slot<T>` struct layout (drops `occupied`). Linking a
raw-compiled object/program against a **checked** runtime object or checked
runtime **bitcode** (or vice versa) is an ABI mismatch - silent memory
corruption. This is the worst kind of failure: not a UB the program author wrote,
but one the *build* introduced.

Therefore raw mode is **whole-program**: the user code, the runtime object, and
any inlined runtime bitcode must ALL be raw, or ALL be checked. There is no
per-translation-unit raw. Mixing must be made impossible, not merely discouraged.

## 2. Driver design (proposed)

A driver flag - `pgy <src> --raw-slots` (working name) - sets raw mode for the
whole compilation:

1. **Emitted-C compile**: add `-DPGY_RAW_SLOTS` (compiler.c flag list).
2. **Runtime object**: build `pgy_runtime_lib.c` with `-DPGY_RAW_SLOTS`, under a
   **distinct cache key** (e.g. `pgy_runtime_cache_<opt>_raw.obj`) so a raw
   program never picks up the checked cached object and vice versa
   (compiler_llvm.c / compiler_runtime_cache.c).
3. **Bitcode inlining**: in raw mode, **disable** `llvm_link_runtime_bitcode`
   (the prebuilt `.bc` is checked) - the runtime comes solely from the raw object.
   (Simpler and safer than maintaining a parallel raw `.bc`.)
4. **Stamp the artifact**: record the mode in the output (a symbol / section) so a
   later link of two objects with mismatched modes fails loudly rather than
   corrupting. (Mix-safety enforcement - the load-bearing requirement.)

## 3. Why a flag and not just the macro

The macro alone (manually `-DPGY_RAW_SLOTS`) is a footgun: it is trivially
possible to compile the user code raw and link the default checked runtime -
corruption. The flag's job is to make raw mode *consistent by construction* (all
three sites raw + a mismatch tripwire), so the opt-out cannot silently break the
ABI it opts out of.

## 4. Implementation order (verification-first)

1. Cache-key separation + the `--raw-slots` flag wiring (no behaviour yet beyond
   the define reaching all three sites).
2. The mismatch tripwire (stamp + link-time check) - **before** advertising the
   flag, because without it raw mode is more dangerous than no flag.
3. End-to-end test: a raw program runs correctly (slot ops are bare), a raw
   program linked against a checked runtime is **rejected** (tripwire fires), and
   a checked program is unaffected.

Until step 2 lands, raw mode stays an expert macro with the documented no-mix
caveat, not a surfaced flag - because shipping the opt-out without the mix
tripwire would reintroduce exactly the build-introduced-corruption class this
doc exists to prevent.

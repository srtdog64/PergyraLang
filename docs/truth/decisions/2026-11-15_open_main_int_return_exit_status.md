# OPEN 2026-11-15: does a value returned from `Main` set the process exit status?

Asked by: the Claude lane. Not decided; no route is changed.

## The case

```
func Main() -> Int {
    Log(7);
    return 3;
}
```

Exit status by leg, observed 2026-11-15 in `Ubuntu-C-WSL` with `pgy` and
`pgy-self-driver` built from main `a7d7655e1` (`/root/sd-a7`). stdout was
the same on every leg that built.

| Program | native C | native LLVM | self-host C | self-host LLVM |
|---|---|---|---|---|
| `-> Int`, `return 3` | 0 | 0 | 3 | refused |
| `-> Int`, `return 300` | 0 | 0 | 44 | refused |
| `-> Int`, helper call, `return v` (v = 4) | 0 | 0 | 4 | 0 |
| `-> Bool`, `return true` | 0 | 0 | 1 | refused |
| `-> String`, `return "done"` | 0 | 0 | C compile error | refused |
| `-> Int`, ends in `Exit(5)` | 5 | 5 | 5 | refused |
| `-> Void` | 0 | 0 | 0 | 0 |

Where each status comes from:

- Native C: the `main` wrapper in `src/codegen/transpiler.c` calls Main as a
  statement and returns 0.
- Native LLVM: `llvm_main_emit_user_main` in
  `src/codegen/llvm/llvm_main_wrapper.c` drops the call result; the wrapper
  returns `i32 0`.
- Self-host C: `src/self_hosted/codegen/emission/function_emit.pgy` emits
  Main's body as the body of C `main` (`body_ret` becomes `int`), so a
  `return v;` in Main returns from `main`. Linux keeps `v mod 256`, a Bool
  becomes 0 or 1, and a String does not compile.
- Self-host LLVM returns 0 where it builds.

What the documents say: `docs/grammar/01_syntax.md` gives `Exit(code)`
(result type `Never`) as the way to end the process with a status. Every
Main in the grammar documents, the project scaffold
(`src/compiler/driver_scaffold_project.c`), `src/compiler/pkg.c` and the
REPL is `-> Void`. No document says what a value returned from Main does,
and entrypoint selection
(`src/self_hosted/semantic/ast_entrypoint_selection_policy_owner.pgy`)
selects by name without checking the return type. 683 files under `tests/`
and 57 under `src/` declare `func Main() -> Int`; 24 declare a Bool, String
or Float Main.

## The choice

| | Drop the value (native today) | `Int` is the status | Main must be `Void` |
|---|---|---|---|
| Rule | a value returned from Main is dropped; the status is 0 unless `Exit` sets it | `Main() -> Int` returns its value as the status | Main is `-> Void`; `Exit` sets any other status |
| Work | self-host C calls Main and returns 0, as the native wrapper does | native C, native LLVM and self-host LLVM return the value; the range (8 bits on Linux, 32 on Windows) and Bool/String Main need their own rule | both front ends refuse a non-Void Main; about 760 test and source files are rewritten |
| Legs that change | self-host C | three | none at run time, all at admission |

The Claude lane recommends dropping the value. Both native backends do it
now, `Exit` already owns the status and is typed `Never`, and the self-host
C behavior follows from emitting Main's body inside `main`: it turns 300
into 44 and cannot build a String Main. Once decided, the leg that differs
is fixed at its entry wrapper and a gate compiles and runs each program on
all four legs and compares the exit status.

## Also found (separate from this choice)

Self-host LLVM refuses the small programs marked "refused" above with
`CODEGEN ERROR: direct MIR literal Log program envelope is invalid`. The
literal Log route takes a one-routine program with one two-instruction
block, then dies because the routine is not `-> Void`
(`src/self_hosted/compiler/direct_mir/direct_mir_literal_log_plan_owner.pgy`).
Native admits these programs, so V1 needs this fixed whichever rule is
chosen. Until then an exit-status gate cannot include them on that leg.

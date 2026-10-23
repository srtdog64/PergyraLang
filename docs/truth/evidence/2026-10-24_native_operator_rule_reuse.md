# Native operator vocabulary and LLVM precedence — run record

Observed 2026-10-24. This is local, bounded compiler evidence, not V1 closure
or published-SHA CI. The unrelated shared working-tree changes were excluded.

## Source and executable identity

- Base: `ad920678bdaef3865f2d27f88e47a9cbb1d7d83e`.
- Vocabulary commit: `646e74af757c6c8508b9fc478fb3b1ec01852173`.
- Final source: vocabulary commit plus the five-line free-declaration guard in
  `llvm_domain_role_emit.c`, runtime gate, Makefile wiring and this receipt's
  commit. No other production changes were copied to the validation export.
- Export: `/tmp/pgy-operator-reuse.qAlfP0`, Ubuntu-C-WSL; GCC 13.3.0,
  LLVM/Clang 18.1.3. Not a Git worktree or the installed compiler.
- Native Windows validation below is the standalone vocabulary unit only.

| Executable | SHA-256 |
|---|---|
| `bin/pgy-baseline` | `b127f8f46a303df7bffd7e46d844fb87ab1a2fdee53bdfc4b6f912f22fa6ce97` |
| `bin/pgy-vocabulary` | `da05f61f37f60a0a0e73d88345db014753fffd7adc0c680e5f089fbd16ee5b86` |
| `bin/pgy-repaired` and final `bin/pgy` | `d538fb6a2926c9831be78944291d3b92dcd9922d4340cbcad2d60ec9c077a61d` |
| `bin/pgy-alias-mutant` | `d6c76c65f4a9f7c9623eed3da4138f2ccdf18f5307e6da373932988a07adb2e1` |

## Observed gates

| Gate | Observed result |
|---|---|
| `operator-overload-rules-test` | 9,132 checks PASS |
| same standalone unit, Windows UCRT GCC | 9,132 checks PASS |
| same standalone unit, Linux ASan+UBSan with leak detection | 9,132 checks PASS; not whole-compiler sanitizer coverage |
| `test-semantic` | 1,014 passed, 0 failed, vocabulary and final source |
| `test-transpile` | 3,240 passed, 0 failed, vocabulary and final source |
| runtime gate, baseline and vocabulary-only | 22 exact-output runs + 18 semantic rejections pass; two LLVM priority cases fail |
| `operator-overload-rules-test-smoke`, final | 24 exact-output runs + 18 semantic rejections PASS on C/LLVM |
| intentional `AddExtra` admission | unit fails; both C/LLVM negative runtime legs fail because invalid overload is admitted |
| unknown `--case` selector | nonzero exit; no empty-suite success |

The source generator independently spells all 72 role aliases and all 11 free
function suffixes. It checks fixed numeric outputs (not C == LLVM alone),
ordinary builtin operators, and free-function priority in both declaration
orders. Negatives require a semantic `PGY_SEM_BINOP_TYPE_MISMATCH` and no output
executable, covering case sensitivity, prefix/suffix near matches, another
operator's alias, wrong arity/type and unsupported operators.

The original priority counterexample had `IntMath.Add` return 321 and
`operator_add_Int` return 789. C returned 789, LLVM returned 321. LLVM was
materializing a role bridge into an existing free declaration before that
function's body was emitted. The guard queries the existing MIR declaration
inventory; final C and LLVM both return 789 in both declaration orders.

The alias mutant adds `AddExtra` to the plus row. It is built only in the
temporary export. The unit reports its near-name assertion, and both runtime
negative legs report `invalid overload admitted`. The original owner was
restored, rebuilt, and all final focused/integration unit gates rerun.

## Broader gate is still red

`make -j2 native-compiler-boundary-regression-test-smoke` failed in
`tests/string_result_control_ownership_smoke.sh` on its fresh factory case:

```text
ArrayDropOwnedStrings cannot release 'fields': shallow mutation withdrew its owned String-element premise
```

The exact same failure was reproduced with `bin/pgy-baseline`; no ownership
policy or fixture was changed to hide it. Single-evaluation match, C callable
macro hygiene and imported-private-nominal boundary tests passed. The new
operator runtime gate passed independently, but the combined boundary target
and full CI are **not green**. Logs: `final-boundary.log`,
`baseline-string-boundary.log`, `final-integration.log` in the export.

## Separate ABI falsifier remains open

Both baseline and final native C reject this semantically admitted source at
native compilation: emitted `Box` value is passed to a `Box *` free-function
parameter. This is not corrected by vocabulary reuse or the LLVM priority fix.

```pergyra
subject Box {}
ability Arithmetic { func Add(self, rhs: Int) -> Int; }
role BoxMath for Box {
    impl Arithmetic { func Add(self, rhs: Int) -> Int { return 321; } }
}
func operator_add_Box(a: Box, b: Int) -> Int { return 789; }
func Main() -> Void { let result: Int = Box() + 2; Log(result); }
```

Reproduce with `--native-pipeline --backend=c`; logs are
`subject-baseline.log` / `subject-repaired.log`. The missing fact/consumer is
free-operator receiver argument carriage in `transpiler_expr_core_emit.c`;
do not weaken subject identity or use this failure as a successful negative.
Native sealed per-expression operator target carriage, self-host parity,
canonical admission, bootstrap, installed-driver and published CI were not
validated by this slice. Push remains with the main integration worker.

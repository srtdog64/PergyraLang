import re
import sys


source_path, backend, mode, output_path = sys.argv[1:]
with open(source_path, encoding="utf-8") as source_file:
    text = source_file.read()

if backend == "c":
    pattern = r'printf\("%s\\n", "long-division-ready"\);'
    values = {
        "positive":
            'printf("%lld\\n", pgy_scalar_routine_1(100LL, 7LL));',
        "minimum-minus-one":
            'printf("%lld\\n", pgy_scalar_routine_1('
            '(-9223372036854775807LL - 1), -1LL));',
        "zero-divisor":
            'printf("%lld\\n", pgy_scalar_routine_1(100LL, 0LL));',
    }
elif backend == "llvm":
    pattern = (
        r"  call i32 \(ptr, \.\.\.\) @printf\(ptr getelementptr inbounds "
        r"\(\[4 x i8\], ptr @\.pgy\.scalar\.cfg\.string\.format, i64 0, "
        r"i64 0\), ptr @pgy\.scalar\.string\.0\)"
    )

    def llvm_call(left, right):
        return (
            "  %pgy.long.division = call i64 @pgy.scalar.routine.1("
            f"i64 {left}, i64 {right})\n"
            "  call i32 (ptr, ...) @printf(ptr getelementptr inbounds "
            "([6 x i8], ptr @.pgy.scalar.cfg.int.format, i64 0, i64 0), "
            "i64 %pgy.long.division)"
        )

    values = {
        "positive": llvm_call("100", "7"),
        "minimum-minus-one": llvm_call("-9223372036854775808", "-1"),
        "zero-divisor": llvm_call("100", "0"),
    }
else:
    raise SystemExit(f"unknown backend: {backend}")

if mode not in values:
    raise SystemExit(f"unknown mode: {mode}")
text, count = re.subn(pattern, lambda _: values[mode], text)
if count != 1:
    raise SystemExit(f"expected one target call, replaced {count}")
with open(output_path, "w", encoding="utf-8", newline="\n") as output_file:
    output_file.write(text)

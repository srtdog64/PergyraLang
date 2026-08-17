import re
import sys


source_path, backend, mode, output_path = sys.argv[1:]
with open(source_path, encoding="utf-8") as source_file:
    text = source_file.read()

if backend == "c":
    pattern = r'printf\("%s\\n", "long-multiplication-ready"\);'
    values = {
        "ordinary":
            'printf("%lld\\n", pgy_scalar_routine_1(6LL, 7LL));',
        "overflow":
            'printf("%lld\\n", pgy_scalar_routine_1('
            '9223372036854775807LL, 2LL));',
    }
elif backend == "llvm":
    pattern = (
        r"  call i32 \(ptr, \.\.\.\) @printf\(ptr getelementptr inbounds "
        r"\(\[4 x i8\], ptr @\.pgy\.scalar\.cfg\.string\.format, i64 0, "
        r"i64 0\), ptr @pgy\.scalar\.string\.0\)"
    )

    def llvm_call(left, right):
        return (
            "  %pgy.long.multiplication = call i64 @pgy.scalar.routine.1("
            f"i64 {left}, i64 {right})\n"
            "  call i32 (ptr, ...) @printf(ptr getelementptr inbounds "
            "([6 x i8], ptr @.pgy.scalar.cfg.int.format, i64 0, i64 0), "
            "i64 %pgy.long.multiplication)"
        )

    values = {
        "ordinary": llvm_call("6", "7"),
        "overflow": llvm_call("9223372036854775807", "2"),
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

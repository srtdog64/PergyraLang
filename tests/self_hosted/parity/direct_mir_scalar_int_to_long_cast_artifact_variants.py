import re
import sys


source_path, backend, mode, output_path = sys.argv[1:]
with open(source_path, encoding="utf-8") as source_file:
    text = source_file.read()

if backend == "c":
    pattern = r'printf\("%s\\n", "int-to-long-cast-ready"\);'
    values = {
        "ordinary": (
            'printf("%lld\\n", pgy_scalar_routine_1(-17LL));\n'
            '    printf("%lld\\n", pgy_scalar_routine_2(536870919LL));'
        ),
        "boundary": (
            'printf("%lld\\n", pgy_scalar_routine_1(-2147483648LL));\n'
            '    printf("%lld\\n", pgy_scalar_routine_2(2147483648LL));'
        ),
    }
elif backend == "llvm":
    pattern = (
        r"  call i32 \(ptr, \.\.\.\) @printf\(ptr getelementptr inbounds "
        r"\(\[4 x i8\], ptr @\.pgy\.scalar\.cfg\.string\.format, i64 0, "
        r"i64 0\), ptr @pgy\.scalar\.string\.0\)"
    )

    def llvm_calls(int_value, long_value):
        return (
            "  %pgy.int.to.long = call i64 @pgy.scalar.routine.1("
            f"i64 {int_value})\n"
            "  call i32 (ptr, ...) @printf(ptr getelementptr inbounds "
            "([6 x i8], ptr @.pgy.scalar.cfg.int.format, i64 0, i64 0), "
            "i64 %pgy.int.to.long)\n"
            "  %pgy.long.to.int = call i64 @pgy.scalar.routine.2("
            f"i64 {long_value})\n"
            "  call i32 (ptr, ...) @printf(ptr getelementptr inbounds "
            "([6 x i8], ptr @.pgy.scalar.cfg.int.format, i64 0, i64 0), "
            "i64 %pgy.long.to.int)"
        )

    values = {
        "ordinary": llvm_calls("-17", "536870919"),
        "boundary": llvm_calls("-2147483648", "2147483648"),
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

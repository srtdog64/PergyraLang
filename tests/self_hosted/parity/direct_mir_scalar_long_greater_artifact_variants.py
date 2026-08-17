import re
import sys


source_path, backend, mode, output_path = sys.argv[1:]
with open(source_path, encoding="utf-8") as source_file:
    text = source_file.read()
values = {
    "true": ((9, 4), (9, 4), (-9, 4)),
    "false": ((4, 9), (7, 7), (9, 4)),
}
if mode not in values:
    raise SystemExit(f"unknown mode: {mode}")
(greater_left, greater_right), (equal_left, equal_right), (
    less_left, less_right) = values[mode]

if backend == "c":
    pattern = r'printf\("%s\\n", "long-greater-ready"\);'
    replacement = (
        'printf("%lld\\n", pgy_scalar_routine_1('
        f'{greater_left}LL, {greater_right}LL) ? 1LL : 0LL);\n'
        '  printf("%lld\\n", pgy_scalar_routine_2('
        f'{equal_left}LL, {equal_right}LL) ? 1LL : 0LL);\n'
        '  printf("%lld\\n", pgy_scalar_routine_3('
        f'{equal_left}LL, {equal_right}LL) ? 1LL : 0LL);\n'
        '  printf("%lld\\n", pgy_scalar_routine_4('
        f'{less_left}LL, {less_right}LL) ? 1LL : 0LL);'
    )
elif backend == "llvm":
    pattern = (
        r"  call i32 \(ptr, \.\.\.\) @printf\(ptr getelementptr inbounds "
        r"\(\[4 x i8\], ptr @\.pgy\.scalar\.cfg\.string\.format, i64 0, "
        r"i64 0\), ptr @pgy\.scalar\.string\.0\)"
    )
    replacement = (
        "  %pgy.long.greater = call i1 @pgy.scalar.routine.1("
        f"i64 {greater_left}, i64 {greater_right})\n"
        "  %pgy.long.greater.int = zext i1 %pgy.long.greater to i64\n"
        "  call i32 (ptr, ...) @printf(ptr getelementptr inbounds "
        "([6 x i8], ptr @.pgy.scalar.cfg.int.format, i64 0, i64 0), "
        "i64 %pgy.long.greater.int)\n"
        "  %pgy.long.equal = call i1 @pgy.scalar.routine.2("
        f"i64 {equal_left}, i64 {equal_right})\n"
        "  %pgy.long.equal.int = zext i1 %pgy.long.equal to i64\n"
        "  call i32 (ptr, ...) @printf(ptr getelementptr inbounds "
        "([6 x i8], ptr @.pgy.scalar.cfg.int.format, i64 0, i64 0), "
        "i64 %pgy.long.equal.int)\n"
        "  %pgy.long.not.equal = call i1 @pgy.scalar.routine.3("
        f"i64 {equal_left}, i64 {equal_right})\n"
        "  %pgy.long.not.equal.int = zext i1 %pgy.long.not.equal to i64\n"
        "  call i32 (ptr, ...) @printf(ptr getelementptr inbounds "
        "([6 x i8], ptr @.pgy.scalar.cfg.int.format, i64 0, i64 0), "
        "i64 %pgy.long.not.equal.int)\n"
        "  %pgy.long.less = call i1 @pgy.scalar.routine.4("
        f"i64 {less_left}, i64 {less_right})\n"
        "  %pgy.long.less.int = zext i1 %pgy.long.less to i64\n"
        "  call i32 (ptr, ...) @printf(ptr getelementptr inbounds "
        "([6 x i8], ptr @.pgy.scalar.cfg.int.format, i64 0, i64 0), "
        "i64 %pgy.long.less.int)"
    )
else:
    raise SystemExit(f"unknown backend: {backend}")

text, count = re.subn(pattern, lambda _: replacement, text)
if count != 1:
    raise SystemExit(f"expected one target call, replaced {count}")
with open(output_path, "w", encoding="utf-8", newline="\n") as output_file:
    output_file.write(text)

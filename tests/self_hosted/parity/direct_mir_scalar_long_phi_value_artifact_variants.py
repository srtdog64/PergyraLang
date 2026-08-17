import re
import sys


source_path, backend, mode, output_path = sys.argv[1:]
with open(source_path, encoding="utf-8") as source_file:
    text = source_file.read()

if mode not in ("true", "false"):
    raise SystemExit(f"unknown mode: {mode}")

if backend == "c":
    pattern = r'printf\("%s\\n", "long-phi-ready"\);'
    replacement = (
        'printf("%lld\\n", pgy_scalar_routine_1('
        + ("true" if mode == "true" else "false")
        + ', 11LL, 29LL, 100LL));'
    )
elif backend == "llvm":
    pattern = (
        r"  call i32 \(ptr, \.\.\.\) @printf\(ptr getelementptr inbounds "
        r"\(\[4 x i8\], ptr @\.pgy\.scalar\.cfg\.string\.format, i64 0, "
        r"i64 0\), ptr @pgy\.scalar\.string\.0\)"
    )
    condition = "1" if mode == "true" else "0"
    replacement = (
        f"  %pgy.long.phi = call i64 @pgy.scalar.routine.1("
        f"i1 {condition}, i64 11, i64 29, i64 100)\n"
        "  call i32 (ptr, ...) @printf(ptr getelementptr inbounds "
        "([6 x i8], ptr @.pgy.scalar.cfg.int.format, i64 0, i64 0), "
        "i64 %pgy.long.phi)"
    )
else:
    raise SystemExit(f"unknown backend: {backend}")

text, count = re.subn(pattern, lambda _: replacement, text)
if count != 1:
    raise SystemExit(f"expected one target call, replaced {count}")
with open(output_path, "w", encoding="utf-8", newline="\n") as output_file:
    output_file.write(text)

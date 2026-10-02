"""Observe generated production writer; mutation restores its former leak."""
import pathlib
import re
import sys

backend, variant, input_name, output_name = sys.argv[1:]
source = pathlib.Path(input_name).read_text(encoding="utf-8")
if variant not in ("positive", "old_guard"):
    raise SystemExit("unknown instrumentation variant")

if backend == "c":
    pattern = re.compile(
        r"^(void [A-Za-z_][A-Za-z_0-9]*JsonOwnedFragmentWriteFile"
        r"\(int32_t [A-Za-z_][A-Za-z_0-9]*, char\* ([A-Za-z_][A-Za-z_0-9]*)\)\n\{)",
        re.MULTILINE,
    )
    matches = list(pattern.finditer(source))
    if len(matches) != 1:
        raise SystemExit("missing/ambiguous typed C writer definition")
    match = matches[0]
    argument = match.group(2)
    hook = f"\n    pgy_json_watch({argument});"
    if variant == "old_guard":
        hook += f"\n    if ({argument}[0] == '\\0') return;"
    source = source[:match.end()] + hook + source[match.end():]
    calls = re.compile(r"^(    [A-Za-z_][A-Za-z_0-9]*JsonOwnedFragmentWriteFile\([^;\n]*\);)$", re.MULTILINE)
    if len(calls.findall(source)) != 2:
        raise SystemExit("missing/ambiguous C writer return observations")
    source = calls.sub(r"\1\n    pgy_json_writer_returned();", source)
    marker = "#include <stdlib.h>\n"
    if source.count(marker) != 1:
        raise SystemExit("missing/ambiguous stdlib include")
    source = source.replace(marker, marker +
        "void pgy_json_watch(void *pointer);\n"
        "void pgy_json_writer_returned(void);\n"
        "void pgy_json_observed_free(void *pointer);\n"
        "#define free pgy_json_observed_free\n", 1)
elif backend == "llvm":
    pattern = re.compile(
        r"^(define void @(?:[A-Za-z_][A-Za-z_0-9]*)?JsonOwnedFragmentWriteFile"
        r"\(i32 %[A-Za-z_0-9.]+, ptr (%[A-Za-z_0-9.]+)\) \{\n"
        r"[A-Za-z_0-9.]+:\n)", re.MULTILINE)
    matches = list(pattern.finditer(source))
    if len(matches) != 1:
        raise SystemExit("missing/ambiguous typed LLVM writer entry")
    match = matches[0]
    argument = match.group(2)
    hook = f"  call void @pgy_json_watch(ptr {argument})\n"
    if variant == "old_guard":
        hook += (f"  %json_empty_byte = load i8, ptr {argument}\n"
                 "  %json_empty = icmp eq i8 %json_empty_byte, 0\n"
                 "  br i1 %json_empty, label %json_skip, label %json_body\n"
                 "json_skip:\n  ret void\njson_body:\n")
    source = source[:match.end()] + hook + source[match.end():]
    calls = re.compile(r"^(  call void @(?:[A-Za-z_][A-Za-z_0-9]*)?JsonOwnedFragmentWriteFile\([^\n]*\))$", re.MULTILINE)
    if len(calls.findall(source)) != 2:
        raise SystemExit("missing/ambiguous LLVM writer return observations")
    source = calls.sub(r"\1\n  call void @pgy_json_writer_returned()", source)
    source += "\ndeclare void @pgy_json_watch(ptr)\ndeclare void @pgy_json_writer_returned()\n"
else:
    raise SystemExit("unknown backend")
pathlib.Path(output_name).write_text(source, encoding="utf-8")

#!/usr/bin/env python3
"""Mutate emitted artifacts, never compiler source, to falsify the memory gate."""
import re
import sys
from pathlib import Path

work = Path(sys.argv[1])
c = (work / "program.c").read_text()
llvm = (work / "program.ll").read_text()
patterns = {
    "c": r"^\s*pgy_array_drop_Int\(&_pgy_ssa_values_1\);\n",
    "llvm": r"^\s*call void @pgy_array_drop_Int\(ptr %values\.1\)\n",
}
for backend, source in (("c", c), ("llvm", llvm)):
    suffix = "c" if backend == "c" else "ll"
    suppressed, count = re.subn(patterns[backend], "", source, flags=re.M)
    if count != 2:
        raise SystemExit(f"{backend}: expected two emitted retirement sites, got {count}")
    (work / f"suppressed.{suffix}").write_text(suppressed)
    if backend == "c":
        marker = r"^(\s*int32_t _pgy_return_value_\d+ = )"
        premature, count = re.subn(marker, r"    pgy_array_drop_Int(&_pgy_ssa_values_1);\n\1", source, count=1, flags=re.M)
    else:
        marker = r"^(\s*%\w+ = call i32 @pgy_slice_get_Int\()"
        premature, count = re.subn(marker, r"  call void @pgy_array_drop_Int(ptr %values.1)\n\1", source, count=1, flags=re.M)
    if count != 1:
        raise SystemExit(f"{backend}: no scalar access for premature-retirement control")
    (work / f"premature.{suffix}").write_text(premature)

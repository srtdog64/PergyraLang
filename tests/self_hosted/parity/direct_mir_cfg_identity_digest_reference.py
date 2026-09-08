"""Independent byte/array identity oracle and bounded source/emission checks."""
import pathlib
import re
import sys

source = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
body = source.split("func DirectMirCfgHashString(", 1)[1].split("\nfunc ", 1)[0]
if body.count("StringLength(value)") != 1:
    raise SystemExit("[self-host-cfg-identity-digest] repeated immutable string-length scan")
if "StringLength(" in body.split("while ", 1)[1]:
    raise SystemExit("[self-host-cfg-identity-digest] hash loop rescans string length")
if len(sys.argv) == 2:
    raise SystemExit(0)
emitted = pathlib.Path(sys.argv[2]).read_text(encoding="utf-8")
match = re.search(r"\nint64_t DirectMirCfgHashString\([^;]+?\n\{(.*?)\n\}", emitted, re.S)
if not match or match.group(1).count("pgy_strlen(value)") != 1:
    raise SystemExit("[self-host-cfg-identity-digest] emitted owner repeats the length scan")


def digest(seed, value):
    data = value.encode("utf-8")
    result = (seed * 131 + len(data)) % 268435456
    for byte in data:
        result = (result * 131 + byte) % 268435456
    return result


def hash_int(seed, value):
    return (seed * 131 + value + 2) % 268435456


cases = [(0, ""), (17, ""), (268435455, "ASCII-identity"),
         (1234567, 'quote:" slash:\\ newline:\n'), (31, "한글🙂"), (17, "a" * (1 << 20))]
values = [digest(seed, value) for seed, value in cases]
values.append(1073741824 + values[-1])
integers = hash_int(31, 3)
for value in (1, -2, 7):
    integers = hash_int(integers, value)
strings = hash_int(37, 2)
for value in ("a", "한글🙂"):
    strings = digest(strings, value)
values.extend((integers, strings))
pathlib.Path(sys.argv[3]).write_bytes("".join(f"{value}\n" for value in values).encode("utf-8"))

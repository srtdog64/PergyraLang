#!/usr/bin/env bash
set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 \
    || ! command -v find >/dev/null 2>&1 \
    || ! command -v grep >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[source-utf8] $*" >&2
    exit 1
}

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    fi
fi

if [[ -n "$PYTHON_BIN" ]]; then
    "$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
replacement = b"\xef\xbf\xbd"

for path in sorted((root / "src").rglob("*")):
    if path.suffix not in (".c", ".h"):
        continue
    data = path.read_bytes()
    rel = path.relative_to(root).as_posix()
    try:
        data.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise SystemExit(f"[source-utf8] {rel} is not valid UTF-8: {exc}") from exc
    if replacement in data:
        raise SystemExit(f"[source-utf8] {rel} contains Unicode replacement characters")
PY
    echo "[source-utf8] src .c/.h files are valid UTF-8"
    exit 0
fi

if command -v perl >/dev/null 2>&1; then
    export ROOT_DIR
    find "$ROOT_DIR/src" -type f \( -name '*.c' -o -name '*.h' \) -print0 |
        perl -MEncode=decode,FB_CROAK -0ne '
            chomp;
            my $path = $_;
            open my $fh, "<:raw", $path or die "[source-utf8] cannot read $path: $!\n";
            local $/;
            my $data = <$fh>;
            my $rel = $path;
            my $root = $ENV{"ROOT_DIR"} . "/";
            $rel =~ s/^\Q$root\E//;
            if (index($data, pack("C*", 0xEF, 0xBF, 0xBD)) >= 0) {
                die "[source-utf8] $rel contains Unicode replacement characters\n";
            }
            eval { decode("UTF-8", $data, FB_CROAK); 1 }
                or die "[source-utf8] $rel is not valid UTF-8: $@\n";
        '
    echo "[source-utf8] src .c/.h files are valid UTF-8"
    exit 0
fi

if command -v perl >/dev/null 2>&1; then
    utf8_validator="perl"
elif command -v iconv >/dev/null 2>&1; then
    utf8_validator="iconv"
else
    fail "missing UTF-8 validator (perl or iconv)"
fi

while IFS= read -r -d '' path; do
    rel="${path#"$ROOT_DIR/"}"
    if [[ "$utf8_validator" == "perl" ]]; then
        perl -MEncode -0777 -ne 'Encode::decode("UTF-8", $_, Encode::FB_CROAK)' \
            "$path" >/dev/null || fail "$rel is not valid UTF-8"
    else
        iconv -f UTF-8 -t UTF-8 <"$path" >/dev/null \
            || fail "$rel is not valid UTF-8"
    fi
    if LC_ALL=C grep -q $'\357\277\275' "$path"; then
        fail "$rel contains Unicode replacement characters"
    fi
done < <(find "$ROOT_DIR/src" -type f \( -name '*.c' -o -name '*.h' \) -print0)

echo "[source-utf8] src .c/.h files are valid UTF-8"

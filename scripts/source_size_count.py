#!/usr/bin/env python3
"""Source-size metric: LF records minus nonblank lexical comment-only records.

Blank records, inline code, strings/docstrings/heredoc payloads and shebangs
count. Caps remain owned by their callers. Plain data has no comment syntax.
"""
from __future__ import annotations

import argparse
import io
import re
import shlex
import sys
import tokenize
from pathlib import Path


def records(text: str) -> list[str]:
    if not text:
        return []
    rows = text.split("\n")
    if rows[-1] == "":
        rows.pop()
    return [row.removesuffix("\r") for row in rows]


def c_family_count(rows: list[str], pergyra: bool) -> int:
    block = False
    quote = ""
    continued_comment = False
    total = 0
    for row_number, row in enumerate(rows):
        commented, code = block or continued_comment, bool(quote)
        if continued_comment:
            continued_comment = row.endswith("\\")
            total += int(not row.strip())
            continue
        index = 0
        while index < len(row):
            if not pergyra and not quote and index == len(row) - 2 and row.endswith("\\"):
                next_row = row_number + 1
                while next_row < len(rows) and rows[next_row] == "\\":
                    next_row += 1
                tail = rows[next_row][:1] if next_row < len(rows) else ""
                if (not block and row[index] == "/" and tail in {"/", "*"}) or (block and row[index] == "*" and tail == "/"):
                    raise ValueError("unsupported C comment delimiter formed by line splice")
            if block:
                commented = True
                if row.startswith("*/", index):
                    block, index = False, index + 2
                else:
                    index += 1
            elif quote:
                code = True
                if row[index] == "\\":
                    index += 2
                elif row.startswith(quote, index):
                    index += len(quote)
                    quote = ""
                else:
                    index += 1
            elif row.startswith("//", index):
                commented = True
                continued_comment = not pergyra and row.endswith("\\")
                break
            elif row.startswith("/*", index):
                commented, block, index = True, True, index + 2
            elif pergyra and row.startswith('"""', index):
                code, quote, index = True, '"""', index + 3
            elif row[index] in "\"'":
                code, quote, index = True, row[index], index + 1
            else:
                code |= not row[index].isspace()
                index += 1
        total += int(not row.strip() or code or not commented)
    if block or quote:
        raise ValueError("unterminated block comment or string literal")
    return total


def python_count(text: str, rows: list[str]) -> int:
    excluded = set()
    try:
        for token in tokenize.generate_tokens(io.StringIO(text).readline):
            if token.type == tokenize.COMMENT:
                line, column = token.start
                if not rows[line - 1][:column].strip():
                    if line != 1 or not token.string.startswith("#!"):
                        excluded.add(line)
            if token.type == tokenize.ERRORTOKEN and token.string in "\"'":
                raise ValueError("unterminated Python string literal")
    except (tokenize.TokenError, IndentationError) as error:
        raise ValueError(f"invalid Python lexical input: {error}") from error
    return len(rows) - len(excluded)


def shell_count(rows: list[str]) -> int:
    """Preserve quoted words and queued, quote-removed heredoc delimiters.

    Nested expansions inside a quoted word are conservatively kept as payload.
    Case grammar inside command substitution is explicitly refused: a pattern's
    ')' is not an expansion terminator and requires a full shell grammar owner.
    """
    quote = ""
    escape_single = False
    expansions: list[list[str | int]] = []
    pending: list[tuple[str, bool]] = []
    arithmetic = 0
    total = 0
    row_number = 0
    while row_number < len(rows):
        row = rows[row_number]
        if pending:
            delimiter, strip_tabs = pending[0]
            if (row.lstrip("\t") if strip_tabs else row) == delimiter:
                pending.pop(0)
            total += 1
            row_number += 1
            continue
        physical_rows = 1
        index = 0
        commented, code = False, bool(quote) or arithmetic > 0 or any(frame[0] for frame in expansions)
        queued = []
        while index < len(row):
            char = row[index]
            if not quote and expansions and re.match(r"(?:case|esac)(?=$|[ \t;()])", row[index:]) and (index == 0 or row[index - 1] in " \t;()"):
                raise ValueError("unsupported shell case grammar inside command substitution")
            if quote != "'" and row.startswith("$(", index) and not row.startswith("$((", index):
                expansions.append([quote, ")", 1])
                quote = ""
                code, index = True, index + 2
            elif quote != "'" and row.startswith("${", index):
                expansions.append([quote, "}", 1])
                quote = ""
                code, index = True, index + 2
            elif quote:
                code = True
                if char == "\\" and (quote != "'" or escape_single):
                    if index == len(row) - 1 and row_number + physical_rows < len(rows):
                        row = row[:-1] + rows[row_number + physical_rows]
                        physical_rows += 1
                    else:
                        index += 2
                elif char == quote:
                    quote, index = "", index + 1
                else:
                    index += 1
            elif char == "\\":
                if index == len(row) - 1 and row_number + physical_rows < len(rows):
                    row = row[:-1] + rows[row_number + physical_rows]
                    physical_rows += 1
                else:
                    code, index = True, index + 2
            elif char in "\"'`":
                escape_single = char == "'" and index > 0 and row[index - 1] == "$"
                code, quote, index = True, char, index + 1
            elif row.startswith("((", index):
                arithmetic += 1
                code, index = True, index + 2
            elif arithmetic and row.startswith("))", index):
                arithmetic -= 1
                code, index = True, index + 2
            elif expansions and not arithmetic and char == expansions[-1][1]:
                expansions[-1][2] -= 1
                if expansions[-1][2] == 0:
                    quote = str(expansions.pop()[0])
                code, index = True, index + 1
            elif expansions and not arithmetic and char == ("(" if expansions[-1][1] == ")" else "{"):
                expansions[-1][2] += 1
                code, index = True, index + 1
            elif not arithmetic and row.startswith("<<<", index):
                code, index = True, index + 3
            elif not arithmetic and row.startswith("<<", index):
                start = index + 2
                strip_tabs = start < len(row) and row[start] == "-"
                start += int(strip_tabs)
                rest = row[start:].lstrip()
                lexer = shlex.shlex(rest, posix=True, punctuation_chars=";&|()<>")
                lexer.whitespace_split = True
                lexer.commenters = ""
                try:
                    delimiter = next(lexer, "")
                except ValueError as error:
                    raise ValueError("invalid shell heredoc delimiter") from error
                quoted_delimiter = bool(rest) and rest[0] in "'\"\\"
                if not delimiter or (delimiter in {";", "|", "&", "(", ")"} and not quoted_delimiter):
                    raise ValueError("missing shell heredoc delimiter")
                queued.append((delimiter, strip_tabs))
                code, index = True, start
            elif char == "#" and (index == 0 or row[index - 1] in " \t;|&()"):
                commented = True
                code |= index == 0 and row.startswith("#!")
                break
            else:
                code |= not char.isspace()
                index += 1
        if not row.strip() or code or not commented:
            total += physical_rows
        pending.extend(queued)
        row_number += physical_rows
    if quote or pending or expansions or arithmetic:
        raise ValueError("unterminated shell quote or heredoc")
    return total


def count_text(text: str, suffix: str) -> int:
    rows = records(text.removeprefix("\ufeff"))
    if suffix in {".c", ".h", ".pgy"}:
        return c_family_count(rows, suffix == ".pgy")
    if suffix == ".py":
        return python_count(text.removeprefix("\ufeff"), rows)
    if suffix in {".sh", ".bash"}:
        return shell_count(rows)
    # Explicit plain-data dialects: no guessed comment syntax.
    if suffix in {".txt", ".tsv", ".def", ".json", ".md", ".csv"}:
        return len(rows)
    raise ValueError(f"unsupported source-size dialect: {suffix or '(none)'}")


def count_file(path: Path) -> int:
    if not path.is_file():
        raise ValueError("not a regular readable file")
    return count_text(path.read_bytes().decode("utf-8-sig"), path.suffix.lower())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="*")
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--caps", action="store_true", help="stdin: cap TAB path")
    parser.add_argument("--rows", action="store_true", help="emit count + path")
    parser.add_argument("--total", action="store_true", help="emit sum only after every source was measured")
    parser.add_argument("--paths0", action="store_true", help="NUL-separated stdin paths")
    args = parser.parse_args()
    failed = False
    cache: dict[str, int] = {}

    def measured(rel: str) -> int:
        if rel not in cache:
            cache[rel] = count_file(args.root / rel)
        return cache[rel]

    if args.caps:
        if args.paths or args.paths0 or args.rows or args.total:
            parser.error("--caps cannot be combined with paths/--paths0/--rows/--total")
        requested = 0
        for row in sys.stdin:
            row = row.removesuffix("\n").removesuffix("\r")
            cap, separator, rel = row.partition("\t")
            if not separator or not re.fullmatch(r"0|[1-9][0-9]*", cap) or not rel:
                print(f"[source-size] invalid line-count request: {row}", file=sys.stderr)
                failed = True
                continue
            requested += 1
            try:
                count = measured(rel)
            except (OSError, ValueError, UnicodeError) as error:
                print(f"[source-size] unreadable line-count input: {rel}: {error}", file=sys.stderr)
                failed = True
                continue
            if count > int(cap):
                print(f"[source-size] {rel} has {count} lines; cap is {cap} (comments excluded)", file=sys.stderr)
                failed = True
        if not requested:
            print("[source-size] no line-count requests", file=sys.stderr)
            failed = True
    else:
        if args.rows and args.total:
            parser.error("--rows cannot be combined with --total")
        paths = args.paths
        if args.paths0:
            if paths:
                parser.error("--paths0 cannot be combined with positional paths")
            paths = [path.decode("utf-8") for path in sys.stdin.buffer.read().split(b"\0") if path]
        if not paths:
            parser.error("no source paths")
        total = 0
        for rel in paths:
            try:
                count = measured(rel)
                total += count
                if not args.total:
                    print(f"{count} {rel}" if args.rows else count)
            except (OSError, ValueError, UnicodeError) as error:
                print(f"[source-size] unreadable source: {rel}: {error}", file=sys.stderr)
                failed = True
        if args.total and not failed:
            print(total)
    return int(failed)


if __name__ == "__main__":
    sys.exit(main())

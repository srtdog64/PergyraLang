#!/usr/bin/env python3
"""Independent lexical and CLI falsifiers for the shared size metric.

Run: python tests/source_size_count_test.py -v
Inputs and CLI stdout/stderr are retained under a unique .tmp directory.
This test imports the real counter; it does not implement another lexer.
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
COUNTER = ROOT / "scripts" / "source_size_count.py"
SPEC = importlib.util.spec_from_file_location("source_size_count_under_test", COUNTER)
assert SPEC is not None and SPEC.loader is not None
OWNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(OWNER)

NESTED_SHELL_CASES = (
    ("nested-command.sh", 'printf \'%s\' "$(printf \'%s\' "\n# payload\n")"\n# outside\n', 3),
    ("nested-parameter.sh", 'printf \'%s\' "${missing:-"\n# payload\n"}"\n# outside\n', 3),
    ("nested-command-heredoc.sh", 'printf \'%s\' "$(cat <<\'EOF\'\n# payload\nEOF\n)"\n# outside\n', 4),
    ("nested-two-commands.sh", 'printf \'%s\' "$(printf \'%s\' "$(printf \'%s\' "\n# payload\n")")"\n# outside\n', 3),
)
UNSUPPORTED_QUOTED_CASE = 'value="$(case x in\nx) printf \'%s\' "\n# payload\n";;\nesac\n)"\n# outside\n'
PHASE2_C_REFUSALS = (
    ("splice-block-start.c", "/\\\n* comment */\n"),
    ("splice-line-start.h", "/\\\n/ comment\n"),
    ("splice-block-end.c", "/* comment\n*\\\n/\n"),
    ("splice-prefixed-end.h", "/* comment *\\\n/\n"),
    ("splice-chained-start.c", "/\\\n\\\n* comment */\n"),
    ("splice-crlf-start.h", "/\\\r\n/ comment\r\n"),
    ("splice-crlf-end.c", "/* comment\r\n*\\\r\n/\r\n"),
    ("splice-chained-end.h", "/* comment\n*\\\n\\\n/\n"),
)


class LexicalMetricTests(unittest.TestCase):
    def counts(self, suffix: str, cases: tuple[tuple[str, str, int], ...]) -> None:
        for name, text, expected in cases:
            with self.subTest(dialect=suffix, case=name):
                self.assertEqual(OWNER.count_text(text, suffix), expected)

    def test_physical_records_and_encoding(self) -> None:
        self.counts(".pgy", (
            ("empty", "", 0),
            ("one blank", "\n", 1),
            ("whitespace blank", " \t\n", 1),
            ("unterminated final", "value", 1),
            ("terminated final", "value\n", 1),
            ("trailing blank", "value\n\n", 2),
            ("CRLF", "value\r\n\r\nother", 3),
            ("NUL is not a delimiter", "a\0b\n\0", 2),
            ("initial BOM", "\ufeff// comment\nvalue\n", 1),
            ("block blank", "/* comment\n\n \t\ncomment */\n", 2),
        ))

    def test_c_and_header_comment_boundaries(self) -> None:
        cases = (
            ("comment-only and inline", "// line\n/* start\n * text\n\n end */\ncode(); // inline\n", 2),
            ("code after block", "/* a */ call(); /* b */\n/* comment */\n\t\n", 2),
            ("string markers", 'char *s = "// /* */";\n// excluded\n', 1),
            ("escaped quotes", r'char *s = "\"//\"";' + "\n" + r"char q = '\''; // tail" + "\n", 2),
            ("continued line comment", "// comment \\\nhidden(); \\\nstill_hidden();\nreal();\n", 1),
            ("CRLF continuation", "// comment \\\r\nhidden();\r\nreal();\r\n", 1),
            ("blank ending continuation", "// comment \\\n\nreal();\n", 2),
            ("unterminated comment final record", "// comment", 0),
        )
        for suffix in (".c", ".h"):
            self.counts(suffix, cases)

    def test_pergyra_strings_and_no_c_line_splice(self) -> None:
        self.counts(".pgy", (
            ("inline comments", "let n: Int = 1; // note\n/* note */\n", 1),
            ("escaped quotes", r'let s: String = "\"//\" /* */";' + "\n", 1),
            ("triple payload", 'let s = """\n// payload\n/* payload */\n\n"""; // tail\n// comment\n', 5),
            ("escaped triple marker", 'let s = """\n\\""" payload\n/* payload */\n""";\n', 4),
            ("no C splice", "// comment \\\nhidden(); \\\nstill_code();\nreal();\n", 3),
        ))

    def test_c_line_spliced_comment_delimiters_explicitly_refused(self) -> None:
        for suffix in (".c", ".h"):
            for name, text in PHASE2_C_REFUSALS:
                with self.subTest(dialect=suffix, case=name):
                    with self.assertRaisesRegex(ValueError, "unsupported C comment delimiter formed by line splice"):
                        OWNER.count_text(text, suffix)

    def test_c_quoted_spliced_marker_payload_keeps_code(self) -> None:
        for suffix in (".c", ".h"):
            self.counts(suffix, (
                ("quoted block start", 'char *s = "/\\\n* payload";\n', 2),
                ("quoted block end", 'char *s = "*\\\n/ payload";\n', 2),
                ("quoted line start CRLF", 'char *s = "/\\\r\n/ payload";\r\n', 2),
            ))

    def test_python_tokens_preserve_docstrings(self) -> None:
        self.counts(".py", (
            ("comments vs strings", "# comment\nx = '# payload' # inline\n", 1),
            ("docstring is code", '"""doc\n# payload\n\nend"""\n# comment\n', 4),
            ("raw and formatted", 'a = r"# payload"\nb = f"value={a} # payload"\n', 2),
            ("escaped quote", r"a = '\'# payload' # tail" + "\n", 1),
            ("blank comments", "# comment\n\n \t\n# final", 2),
            ("shebang", "#!/usr/bin/env python3\n# comment\nx = 1\n", 2),
            ("BOM shebang", "\ufeff#!/usr/bin/env python3\n# comment\n", 1),
        ))

    def test_shell_quotes_words_and_directive(self) -> None:
        self.counts(".sh", (
            ("shebang", "#!/usr/bin/env bash\n# comment\necho yes # inline\n", 2),
            ("quoted hashes", "printf '%s' '# payload'\nprintf \"# payload\"\n# comment\n", 2),
            ("hash word", "echo value#word\n#word\necho \\#word\n", 2),
            ("escaped quote", 'printf "%s" "escaped \\" # payload" # comment\n', 1),
            ("ANSI escaped single quote", "printf '%s' $'escaped\\' # payload' # outside\n", 1),
            ("ANSI multiline payload", "printf '%s' $'escaped\\'\n# payload\nend'\n# outside\n", 3),
            ("multiline quote", 'printf "%s" "\n# payload\n"\n# comment\n', 3),
            ("here string", "cat <<< '# payload'\n# comment\n", 1),
            ("arithmetic shift", "(( value = 8 << 1 )) # inline\n# comment\n", 1),
            ("blank records", "# comment\n\n \t\n# final", 2),
        ))

    def test_shell_heredoc_payloads_and_queued_delimiters(self) -> None:
        self.counts(".bash", (
            ("literal heredoc", "cat <<EOF\n# payload\n// payload\nEOF\n# outside\n", 4),
            ("quoted delimiter", "cat <<'EOF'\n# payload\nEOF\n# outside\n", 3),
            ("quote removed delimiter", 'cat <<E"OF"\n# payload\nEOF\n', 3),
            ("escaped delimiter", "cat <<\\EOF\n# payload\nEOF\n", 3),
            ("tab strip", "cat <<-EOF\n\t# payload\n\tEOF\n# outside\n", 3),
            ("queued", "cat <<FIRST <<'SECOND'\n# first\nFIRST\n// second\n\nSECOND\n# outside\n", 6),
            ("queued tab strip", "cat <<FIRST <<-SECOND\n# first\nFIRST\n\t# second\n\tSECOND\n", 5),
            ("spaces in delimiter", "cat <<'E OF'\n# payload\nE OF\n", 3),
            ("hash inside delimiter word", "cat <<EOF#word\n# payload\nEOF#word\n# outside\n", 3),
            ("quoted operator delimiter", "cat <<'|'\n# payload\n|\n# outside\n", 3),
            ("escaped operator delimiter", "cat <<\\|\n# payload\n|\n# outside\n", 3),
        ))

    def test_shell_line_continuation_cannot_hide_code_records(self) -> None:
        self.counts(".sh", (
            ("unquoted word continuation", "printf '%s\\n' foo\\\n#payload\n", 2),
            ("double-quoted continuation", 'printf "%s" "foo\\\n#payload"\n', 2),
            ("backslash inside comment does not continue it", "# comment \\\ncontinued\nreal\n", 2),
        ))

    def test_shell_nested_quoted_substitution_preserves_payload(self) -> None:
        self.counts(".sh", NESTED_SHELL_CASES)

    def test_shell_valid_case_grammar_is_explicitly_unsupported(self) -> None:
        # Valid Bash syntax, but deliberately outside this counter's lexer.
        with self.assertRaisesRegex(ValueError, "unsupported shell case grammar"):
            OWNER.count_text(UNSUPPORTED_QUOTED_CASE, ".sh")

    def test_plain_data_does_not_guess_comments(self) -> None:
        text = "# data\n// data\n/* data */\n\nlast"
        for suffix in (".txt", ".tsv", ".def", ".json", ".md", ".csv"):
            with self.subTest(dialect=suffix):
                self.assertEqual(OWNER.count_text(text, suffix), 5)

    def test_unknown_dialects_fail_closed_without_guessing(self) -> None:
        # Only the explicitly named plain-data dialects have a count contract.
        for suffix in (".ini", ".unknown", ""):
            with self.subTest(dialect=suffix):
                with self.assertRaisesRegex(ValueError, "unsupported source-size dialect"):
                    OWNER.count_text("# data\n\nlast", suffix)

    def test_malformed_lexical_input_fails_closed(self) -> None:
        cases = (
            (".c", "/* unterminated\n"),
            (".pgy", 'let s = "unterminated\n'),
            (".pgy", 'let s = """unterminated\n'),
            (".py", 'x = "unterminated\n'),
            (".py", '"""unterminated\n'),
            (".sh", "echo 'unterminated\n"),
            (".sh", "cat <<EOF\n# still payload\n"),
            (".sh", "cat <<\n"),
        )
        for suffix, text in cases:
            with self.subTest(dialect=suffix, input=text):
                with self.assertRaises(ValueError):
                    OWNER.count_text(text, suffix)


class CommandLineMetricTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        scratch = ROOT / ".tmp" / "self_hosted"
        scratch.mkdir(parents=True, exist_ok=True)
        cls.work = Path(tempfile.mkdtemp(prefix="source_size_count.", dir=scratch))
        cls.before = hashlib.sha256(COUNTER.read_bytes()).hexdigest()
        (cls.work / "counter-before.sha256").write_text(cls.before + "\n", encoding="utf-8")
        cls.serial = 0
        print(f"[source-size-count-test] evidence: {cls.work}", file=sys.stderr)

    @classmethod
    def tearDownClass(cls) -> None:
        after = hashlib.sha256(COUNTER.read_bytes()).hexdigest()
        (cls.work / "counter-after.sha256").write_text(after + "\n", encoding="utf-8")
        if after != cls.before:
            raise AssertionError("counter changed during independent tests; rerun required")

    def fixture(self, name: str, text: str | bytes) -> str:
        path = self.work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(text.encode("utf-8") if isinstance(text, str) else text)
        return name

    def cli(self, *arguments: str, stdin: bytes = b"") -> subprocess.CompletedProcess[bytes]:
        command = [sys.executable, str(COUNTER), "--root", str(self.work), *arguments]
        result = subprocess.run(command, input=stdin, capture_output=True, timeout=15,
                                env={**os.environ, "PYTHONIOENCODING": "utf-8"})
        type(self).serial += 1
        evidence = self.work / f"cli-{type(self).serial:03d}"
        evidence.with_suffix(".json").write_text(json.dumps({
            "command": command, "returncode": result.returncode,
            "stdin_hex": stdin.hex(),
        }, ensure_ascii=False, indent=2), encoding="utf-8")
        evidence.with_suffix(".stdout").write_bytes(result.stdout)
        evidence.with_suffix(".stderr").write_bytes(result.stderr)
        return result

    def assert_pass(self, result: subprocess.CompletedProcess[bytes]) -> None:
        self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8", "replace"))
        self.assertEqual(result.stderr, b"")

    def assert_refused(self, result: subprocess.CompletedProcess[bytes], diagnostic: bytes) -> None:
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(diagnostic, result.stderr)

    def test_600_code_plus_many_comments_passes_601_fails(self) -> None:
        self.fixture("600.pgy", "// comment\n" * 900 + "call(); // inline\n" * 600)
        self.fixture("601.pgy", "// comment\n" * 900 + "call();\n" * 600 + "call();")
        self.assert_pass(self.cli("--caps", stdin=b"600\t600.pgy\n"))
        self.assert_refused(self.cli("--caps", stdin=b"600\t601.pgy\n"), b"601 lines; cap is 600")
        result = self.cli("--rows", "600.pgy", "601.pgy")
        self.assert_pass(result)
        self.assertEqual(result.stdout.decode("utf-8").splitlines(), ["600 600.pgy", "601 601.pgy"])

    def test_duplicate_caps_use_tighter_limit_in_either_order(self) -> None:
        self.fixture("duplicates.pgy", "value;\n" * 601)
        for caps in (b"600\tduplicates.pgy\n700\tduplicates.pgy\n",
                     b"700\tduplicates.pgy\n600\tduplicates.pgy\n"):
            with self.subTest(caps=caps):
                self.assert_refused(self.cli("--caps", stdin=caps), b"601 lines; cap is 600")
        self.fixture("duplicates-pass.pgy", "value;\n" * 600)
        self.assert_pass(self.cli("--caps", stdin=b"700\tduplicates-pass.pgy\n600\tduplicates-pass.pgy\n"))

    def test_zero_limit_and_blank_records(self) -> None:
        self.fixture("empty.c", b"")
        self.fixture("comments.c", "// comment\n/* comment */")
        self.fixture("blank.c", "/* comment\n\nend */\n")
        self.assert_pass(self.cli("--caps", stdin=b"0\tempty.c\n0\tcomments.c\n"))
        self.assert_refused(self.cli("--caps", stdin=b"0\tblank.c\n"), b"1 lines; cap is 0")

    def test_total_success_is_exactly_one_sum_record(self) -> None:
        self.fixture("total-a.pgy", "// excluded\nfirst;\n\n")
        self.fixture("total-b.py", "# excluded\nx = '# payload'\ny = 1\nz = 2")
        self.fixture("total-empty.h", b"")
        result = self.cli("--total", "total-a.pgy", "total-b.py", "total-empty.h")
        self.assert_pass(result)
        self.assertEqual(result.stdout.decode("utf-8").splitlines(), ["5"])
        result = self.cli("--total", "total-empty.h")
        self.assert_pass(result)
        self.assertEqual(result.stdout.decode("utf-8").splitlines(), ["0"])

    def test_total_counts_duplicate_requests_not_unique_files(self) -> None:
        self.fixture("total-duplicate.pgy", "// excluded\nvalue;\n\n")
        self.fixture("total-other.txt", "# data\n// data\nlast")
        # Cached measurement must not silently deduplicate requested inputs.
        result = self.cli("--total", "total-duplicate.pgy", "total-other.txt", "total-duplicate.pgy")
        self.assert_pass(result)
        self.assertEqual(result.stdout.decode("utf-8").splitlines(), ["7"])
        result = self.cli("--total", "--paths0", stdin=b"total-duplicate.pgy\0total-duplicate.pgy\0total-other.txt\0")
        self.assert_pass(result)
        self.assertEqual(result.stdout.decode("utf-8").splitlines(), ["7"])

    def test_total_failure_never_emits_partial_sum(self) -> None:
        self.fixture("total-good.c", "// excluded\nint value;\n")
        self.fixture("total-unclosed.pgy", "/* unterminated")
        self.fixture("total-unclosed.py", '"""unterminated')
        self.fixture("total-unclosed.sh", "cat <<EOF\n# payload\n")
        self.fixture("total-unknown.ini", "# data\nlast")
        for bad in ("total-missing.pgy", "total-unclosed.pgy", "total-unclosed.py",
                    "total-unclosed.sh", "total-unknown.ini"):
            for paths in ((bad, "total-good.c"), ("total-good.c", bad),
                          ("total-good.c", bad, "total-good.c")):
                with self.subTest(bad=bad, paths=paths):
                    result = self.cli("--total", *paths)
                    self.assertEqual(result.returncode, 1)
                    self.assertIn(b"unreadable source", result.stderr)
                    self.assertEqual(result.stdout, b"")
        result = self.cli("--total", "--paths0", stdin=b"total-good.c\0total-missing.pgy\0total-good.c\0")
        self.assertEqual(result.returncode, 1)
        self.assertIn(b"unreadable source", result.stderr)
        self.assertEqual(result.stdout, b"")

    def test_total_incompatible_modes_refuse_without_stdout(self) -> None:
        self.fixture("total-valid.pgy", "value;")
        for arguments in (("--total", "--rows", "total-valid.pgy"),
                          ("--total", "--caps"), ("--total",)):
            with self.subTest(arguments=arguments):
                result = self.cli(*arguments)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b"error:", result.stderr)
                self.assertEqual(result.stdout, b"")

    def test_nul_paths_spaces_unicode_bom_crlf_and_no_final_newline(self) -> None:
        one = self.fixture("space name.pgy", b"\xef\xbb\xbf// comment\r\nvalue\r\n\r\nlast")
        two = self.fixture("\ud55c\uae00.py", "#!/usr/bin/env python3\n# comment\nvalue = '# literal'")
        result = self.cli("--paths0", "--rows", stdin=(one + "\0" + two + "\0").encode("utf-8"))
        self.assert_pass(result)
        self.assertEqual(result.stdout.decode("utf-8").splitlines(), [f"3 {one}", f"2 {two}"])
        self.fixture("nul.txt", b"a\0b\r\nlast\0")
        result = self.cli("nul.txt")
        self.assert_pass(result)
        self.assertEqual(result.stdout.strip(), b"2")

    def test_cli_heredoc_hash_word_and_ansi_quote(self) -> None:
        self.fixture("hash-word.sh", "cat <<EOF#word\n# payload\nEOF#word\n# outside\n")
        self.fixture("ansi-quote.sh", "printf '%s' $'escaped\\'\n# payload\nend'\n# outside\n")
        for path in ("hash-word.sh", "ansi-quote.sh"):
            with self.subTest(path=path):
                result = self.cli(path)
                self.assert_pass(result)
                self.assertEqual(result.stdout.strip(), b"3")

    def test_cli_nested_quoted_substitution_preserves_payload(self) -> None:
        for name, text, expected in NESTED_SHELL_CASES:
            with self.subTest(path=name):
                self.fixture(name, text)
                result = self.cli(name)
                self.assert_pass(result)
                self.assertEqual(result.stdout.decode("utf-8").splitlines(), [str(expected)])

    def test_cli_valid_case_grammar_refuses_without_partial_total(self) -> None:
        self.fixture("unsupported-quoted-case.sh", UNSUPPORTED_QUOTED_CASE)
        self.fixture("case-total-good.pgy", "value;")
        result = self.cli("unsupported-quoted-case.sh")
        self.assert_refused(result, b"unsupported shell case grammar")
        self.assertEqual(result.stdout, b"")
        result = self.cli("--total", "case-total-good.pgy", "unsupported-quoted-case.sh")
        self.assertEqual(result.returncode, 1)
        self.assertIn(b"unsupported shell case grammar", result.stderr)
        self.assertEqual(result.stdout, b"")

    def test_cli_unknown_dialects_fail_closed(self) -> None:
        self.fixture("unknown.ini", "# data\n\nlast")
        self.fixture("no-extension", "# data\n\nlast")
        for path in ("unknown.ini", "no-extension"):
            with self.subTest(path=path):
                self.assert_refused(self.cli(path), b"unsupported source-size dialect")
                self.assert_refused(self.cli("--caps", stdin=f"600\t{path}\n".encode()),
                                    b"unsupported source-size dialect")

    def test_missing_directory_and_invalid_utf8_inputs_fail_closed(self) -> None:
        self.fixture("bad-utf8.c", b"\xff\n")
        (self.work / "not-a-file.c").mkdir()
        for path in ("missing.pgy", "not-a-file.c", "bad-utf8.c"):
            with self.subTest(path=path):
                self.assert_refused(self.cli(path), b"unreadable source")
                self.assert_refused(self.cli("--caps", stdin=f"600\t{path}\n".encode()),
                                    b"unreadable line-count input")

    def test_malformed_caps_fail_even_beside_valid_request(self) -> None:
        self.fixture("valid.pgy", "value;")
        for bad in (b"", b"\n", b"600 valid.pgy\n", b"-1\tvalid.pgy\n",
                    b"0600\tvalid.pgy\n", b"1.5\tvalid.pgy\n", b"\tvalid.pgy\n",
                    b"600\t\n", b" 600\tvalid.pgy\n"):
            with self.subTest(request=bad):
                expected = b"no line-count requests" if bad == b"" else b"invalid line-count request"
                self.assert_refused(self.cli("--caps", stdin=bad), expected)
                if bad:
                    self.assert_refused(self.cli("--caps", stdin=b"600\tvalid.pgy\n" + bad), expected)

    def test_cli_malformed_lexical_input_is_not_success(self) -> None:
        self.fixture("bad.pgy", "/* unterminated")
        self.assert_refused(self.cli("bad.pgy"), b"unterminated block comment")
        self.assert_refused(self.cli("--caps", stdin=b"600\tbad.pgy\n"), b"unterminated block comment")

    def test_cli_spliced_c_delimiters_refuse_without_partial_total(self) -> None:
        self.fixture("splice-total-good.c", "value;")
        for name, text in PHASE2_C_REFUSALS:
            with self.subTest(path=name):
                self.fixture(name, text)
                diagnostic = b"unsupported C comment delimiter formed by line splice"
                self.assert_refused(self.cli(name), diagnostic)
                self.assert_refused(self.cli("--caps", stdin=f"600\t{name}\n".encode()), diagnostic)
                result = self.cli("--total", "splice-total-good.c", name)
                self.assertEqual(result.returncode, 1)
                self.assertIn(diagnostic, result.stderr)
                self.assertEqual(result.stdout, b"")

    def test_mutually_exclusive_cli_modes_and_missing_arguments(self) -> None:
        self.fixture("valid.pgy", "value;")
        for arguments in (("--caps", "valid.pgy"), ("--caps", "--rows"),
                          ("--caps", "--paths0"), ("--paths0", "valid.pgy"), ()):
            with self.subTest(arguments=arguments):
                result = self.cli(*arguments)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b"error:", result.stderr)


if __name__ == "__main__":
    program = unittest.main(exit=False)
    evidence = getattr(CommandLineMetricTests, "work", None)
    if evidence is not None:
        (evidence / "unittest-result.json").write_text(json.dumps({
            "tests_run": program.result.testsRun,
            "successful": program.result.wasSuccessful(),
            "failures": [(str(case), trace) for case, trace in program.result.failures],
            "errors": [(str(case), trace) for case, trace in program.result.errors],
        }, ensure_ascii=False, indent=2), encoding="utf-8")
    sys.exit(not program.result.wasSuccessful())

#!/usr/bin/env python3
"""Behavioral boundary tests of the documentation line-size gate."""

from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from docs_truth_size_gate import check_truth_size


class TruthSizeGateTest(unittest.TestCase):
    def setUp(self) -> None:
        self.directory = TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        for family in ("state", "lanes"):
            (self.root / "docs" / "truth" / family).mkdir(parents=True)

    def write_lines(self, relative: str, count: int, newline: str = "\n") -> None:
        path = self.root / "docs" / "truth" / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes((newline.join(["값"] * count)).encode("utf-8"))

    def test_exact_limits_and_no_final_newline(self) -> None:
        self.write_lines("state/current.md", 60)
        self.write_lines("lanes/owner.md", 40, "\r\n")
        self.assertEqual(check_truth_size(self.root), [])

    def test_state_excess_is_rejected(self) -> None:
        self.write_lines("state/current.md", 61)
        errors = check_truth_size(self.root)
        self.assertEqual(len(errors), 1)
        self.assertIn("61 lines exceeds 60", errors[0])

    def test_nested_lane_excess_is_rejected(self) -> None:
        self.write_lines("lanes/nested/owner.md", 41)
        errors = check_truth_size(self.root)
        self.assertEqual(len(errors), 1)
        self.assertIn("41 lines exceeds 40", errors[0])

    def test_missing_input_is_not_green(self) -> None:
        (self.root / "docs" / "truth" / "state").rmdir()
        self.assertEqual(len(check_truth_size(self.root)), 1)

    def test_invalid_encoding_is_not_green(self) -> None:
        path = self.root / "docs" / "truth" / "state" / "bad.md"
        path.write_bytes(b"\xff")
        self.assertEqual(len(check_truth_size(self.root)), 1)


if __name__ == "__main__":
    unittest.main()

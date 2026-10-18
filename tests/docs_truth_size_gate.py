#!/usr/bin/env python3
"""Enforce the physical line budgets owned by docs/truth/README.md."""

from pathlib import Path
import sys


def check_truth_size(root: Path) -> list[str]:
    errors: list[str] = []
    for family, limit in (("state", 60), ("lanes", 40)):
        directory = root / "docs" / "truth" / family
        if not directory.is_dir():
            errors.append(f"missing size-gate input: {directory}")
            continue
        for path in sorted(directory.rglob("*.md")):
            try:
                lines = len(path.read_text(encoding="utf-8").splitlines())
            except (OSError, UnicodeError) as error:
                errors.append(f"cannot read {path}: {error}")
                continue
            if lines > limit:
                errors.append(f"{path.relative_to(root)}: {lines} lines exceeds {limit}")
    return errors


def main() -> int:
    errors = check_truth_size(Path(__file__).resolve().parent.parent)
    for error in errors:
        print(f"[docs-truth-size] {error}", file=sys.stderr)
    if errors:
        return 1
    print("[docs-truth-size] state <= 60 and lanes <= 40 lines")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

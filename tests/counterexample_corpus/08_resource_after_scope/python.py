# Bug: the agent releases the acme search tool handle, then a retried tool call still uses it.
# Python: runs with no warning. The name outlives the with block; io.StringIO checks its own state,
# so the stale use raises ValueError at runtime; the hub logs the failed call to stderr.
import io
import sys
from typing import Self


class ToolHandle:
    def __init__(self, tenant: str, archive: list[str]) -> None:
        self.tenant = tenant
        self.archive = archive
        self.transcript = io.StringIO()

    def record(self, line: str) -> None:
        self.transcript.write(line + "\n")

    def __enter__(self) -> Self:
        return self

    def __exit__(self, *exc: object) -> None:
        self.archive.append(self.transcript.getvalue())  # flush the transcript to storage
        self.transcript.close()


def retry_search(handle: ToolHandle) -> None:
    handle.record("search retry: 1 hit")


archive: list[str] = []
with ToolHandle("acme", archive) as handle:
    handle.record("search: 3 hits")
# the agent's tool session ended with the block, but the name is still bound
try:
    print("BUG_MANIFESTED 08")  # the stale use of the released handle follows
    retry_search(handle)
except Exception as exc:
    print(f"tool call failed: {type(exc).__name__}: {exc}", file=sys.stderr)

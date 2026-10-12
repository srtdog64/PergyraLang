# Bug: the support agent's turn is granted io_write (post the reply) only, but a context tool reads a local file.
# Python: runs with no warning. The grant is runtime data the tool never consults; any code can call
# open(). Audit hooks (sys.addaudithook) can observe the open but are not a sandbox.
import tempfile
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path


class Capability(Enum):
    IO_READ = "io_read"
    IO_WRITE = "io_write"
    NETWORK = "network"


@dataclass
class TurnContext:
    tenant: str
    caps: frozenset[Capability]
    workspace: Path
    outbox: list[str] = field(default_factory=list)


def attach_context(ctx: TurnContext, ticket: str) -> str:
    notes = (ctx.workspace / "admin_notes.txt").read_text(encoding="utf-8")
    if Capability.IO_READ not in ctx.caps:
        print("BUG_MANIFESTED 04")
    return f"{ticket} | {notes}"


def run_agent_turn(ctx: TurnContext, ticket: str) -> None:
    if Capability.IO_WRITE not in ctx.caps:
        raise PermissionError("this turn may not post a reply")
    ctx.outbox.append(f"Re: {attach_context(ctx, ticket)}")


# Host setup: the operator's notes file sits in the agent's workspace.
with tempfile.TemporaryDirectory(prefix="agenthub-") as workspace:
    (Path(workspace) / "admin_notes.txt").write_text("globex renewal at risk", encoding="utf-8")
    turn = TurnContext("acme", frozenset({Capability.IO_WRITE}), Path(workspace))
    run_agent_turn(turn, "acme-42 reset password")

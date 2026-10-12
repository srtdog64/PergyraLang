# Bug: the agent keeps a handle to the world's tool zone, the world rotates to the
# next tenant, and the agent then records a call through the stale handle.
# Python: runs without complaint. A read-only property returns a reference that can outlive the rotation.
from dataclasses import dataclass


@dataclass
class Session:
    tenant: str
    calls: int = 0

    def record(self) -> None:
        self.calls += 1


@dataclass(frozen=True)
class ToolZone:
    session: Session


class AgentWorld:
    def __init__(self, tools: ToolZone) -> None:
        self._tools = tools

    @property
    def tools(self) -> ToolZone:
        return self._tools

    def rotate(self, tenant: str) -> None:
        self._tools = ToolZone(Session(tenant))


world = AgentWorld(ToolZone(Session("acme")))
handle = world.tools
world.rotate("globex")
handle.session.record()
if handle is not world.tools and handle.session.calls == 1:
    print("BUG_MANIFESTED 16")

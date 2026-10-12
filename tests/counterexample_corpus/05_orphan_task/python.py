# Bug: the agent turn starts a background search tool call for tenant acme and returns without joining it.
# Python: runs with no warning. asyncio.create_task schedules the coroutine, so nothing
# reports the missing await; the task keeps running after the turn returns.
import asyncio
from dataclasses import dataclass, field


@dataclass
class Session:
    tenant: str
    open: bool = True
    results: list[int] = field(default_factory=list)


async def search_tool(session: Session) -> int:
    await asyncio.sleep(0.05)  # the tool call outlives the turn
    if not session.open:
        # the result lands in a session the hub already closed
        print("BUG_MANIFESTED 05")
    session.results.append(3)
    return 3


async def draft_reply(session: Session) -> str:
    await asyncio.sleep(0.01)
    return f"reply for {session.tenant}"


async def run_agent_turn(session: Session) -> str:
    asyncio.create_task(search_tool(session))  # started in the background, never awaited
    return await draft_reply(session)


async def main() -> None:
    session = Session("acme")
    await run_agent_turn(session)
    session.open = False  # the hub closes the session once the turn reports completion
    await asyncio.sleep(0.2)  # the hub keeps serving other tenants


asyncio.run(main())

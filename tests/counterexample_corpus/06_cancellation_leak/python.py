# Bug: the user presses stop; the agent cancels the export tool call for tenant acme but never waits for it to finish.
# Python: runs with no warning. task.cancel() interrupts the coroutine at its next await, but a
# blocking write already handed to asyncio.to_thread finishes; nothing requires awaiting the task.
import asyncio
import time
from dataclasses import dataclass, field


@dataclass
class Workspace:
    tenant: str
    export_stopped: bool = False
    files: list[str] = field(default_factory=list)


def write_chunk(ws: Workspace, chunk: int) -> None:
    time.sleep(0.04)  # blocking storage SDK call
    if ws.export_stopped:
        # the user was already told the export stopped
        print("BUG_MANIFESTED 06")
    ws.files.append(f"export-{chunk}.csv")


async def export_tool(ws: Workspace) -> None:
    for chunk in (1, 2, 3):
        await asyncio.to_thread(write_chunk, ws, chunk)


async def stop_export(ws: Workspace) -> None:
    export_call = asyncio.create_task(export_tool(ws))
    await asyncio.sleep(0.01)  # the user presses stop while chunk 1 is being written
    export_call.cancel()
    ws.export_stopped = True  # reported without awaiting export_call


async def main() -> None:
    ws = Workspace("acme")
    await stop_export(ws)
    await asyncio.sleep(0.2)  # the hub keeps serving other tenants


asyncio.run(main())

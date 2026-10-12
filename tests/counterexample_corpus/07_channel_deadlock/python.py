# Bug: the planner waits for the executor's capacity while the executor waits for the plan first; neither proceeds.
# Python: runs with no warning. asyncio does not detect the wait cycle; without the turn
# deadline (asyncio.timeout) the event loop would wait forever.
import asyncio


async def planner(plan_ch: asyncio.Queue[int], capacity_ch: asyncio.Queue[int]) -> None:
    capacity = await capacity_ch.get()
    await plan_ch.put(capacity)


async def executor(plan_ch: asyncio.Queue[int], capacity_ch: asyncio.Queue[int]) -> int:
    plan = await plan_ch.get()  # waits for the plan first
    await capacity_ch.put(3)  # and only then announces its capacity
    return plan


async def main() -> None:
    plan_ch: asyncio.Queue[int] = asyncio.Queue(maxsize=1)
    capacity_ch: asyncio.Queue[int] = asyncio.Queue(maxsize=1)
    try:
        async with asyncio.timeout(2):  # turn deadline
            _, steps = await asyncio.gather(
                planner(plan_ch, capacity_ch), executor(plan_ch, capacity_ch)
            )
    except TimeoutError:
        print("BUG_MANIFESTED 07")
        return
    print(f"acme turn executed {steps} steps")


asyncio.run(main())

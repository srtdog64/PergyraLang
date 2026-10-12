# Bug: the agent fans out one summarize task per tenant document, with no bound on concurrency.
# Python: accepted; asyncio.gather schedules every coroutine at once and nothing limits the count.
import asyncio

MAX_IN_FLIGHT = 8  # what the model gateway allows per tenant
in_flight = 0
reported = False


async def summarize(doc: str) -> int:
    global in_flight, reported
    in_flight += 1
    if in_flight > MAX_IN_FLIGHT and not reported:
        reported = True
        print("BUG_MANIFESTED 09")
    await asyncio.sleep(0.005)  # model call latency
    in_flight -= 1
    return 10 + len(doc) % 5


async def summarize_folder(docs: list[str]) -> int:
    tokens = await asyncio.gather(*(summarize(doc) for doc in docs))
    return sum(tokens)


# acme's folder size comes from the tool call; nothing caps it.
docs = [f"acme/doc-{i}.md" for i in range(64)]
asyncio.run(summarize_folder(docs))

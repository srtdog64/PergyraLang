# Bug: the agent appends every search hit to its result buffer although the request wants the top 20.
# Python: accepted; the list grows until the process runs out of memory.
MAX_BUFFERED = 4096  # per-request buffer budget the host would want (16 KiB of 32-bit ids)
reported = False


def search_hit(rank: int) -> int:
    return 700000 + rank  # record id of the hit at this rank


def collect_hits(matched: int) -> list[int]:
    global reported
    buffer: list[int] = []
    for rank in range(matched):
        buffer.append(search_hit(rank))
        if len(buffer) > MAX_BUFFERED and not reported:
            reported = True
            print("BUG_MANIFESTED 10")
    return buffer


requested = 20
# The search tool matched 20000 acme records and pages through all of them.
top = collect_hits(20000)[:requested]

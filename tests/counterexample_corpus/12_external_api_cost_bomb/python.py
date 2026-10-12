# Bug: the agent retries a paid model call until it succeeds; every attempt is billed.
# Python: accepted; nothing bounds attempts or spend.
import asyncio
from dataclasses import dataclass

PRICE_CENTS = 3
REQUEST_BUDGET_CENTS = 10
upstream_calls = 0
spent_cents = 0
reported = False


@dataclass
class Completion:
    status: int
    text: str | None = None


# Local stand-in for the paid API (no network): overloaded (529) for the first 400 calls.
async def complete_paid(prompt: str) -> Completion:
    global upstream_calls, spent_cents, reported
    upstream_calls += 1
    spent_cents += PRICE_CENTS
    if spent_cents > REQUEST_BUDGET_CENTS and not reported:
        reported = True
        print("BUG_MANIFESTED 12")
    if upstream_calls >= 400:
        return Completion(200, f"summary of {prompt}")
    return Completion(529)


async def complete_with_retry(prompt: str) -> str:
    while True:
        res = await complete_paid(prompt)
        if res.status == 200 and res.text is not None:
            return res.text


asyncio.run(complete_with_retry("acme quarterly report"))

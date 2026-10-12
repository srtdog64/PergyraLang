// Bug: the agent retries a paid model call until it succeeds; every attempt is billed.
// TypeScript: tsc --strict accepts it; nothing bounds attempts or spend.
const PRICE_CENTS = 3;
const REQUEST_BUDGET_CENTS = 10;
let upstreamCalls = 0;
let spentCents = 0;
let reported = false;

type Completion = { status: number; text?: string };

// Local stand-in for the paid API (no network): overloaded (529) for the first 400 calls.
async function completePaid(prompt: string): Promise<Completion> {
  upstreamCalls++;
  spentCents += PRICE_CENTS;
  if (spentCents > REQUEST_BUDGET_CENTS && !reported) {
    reported = true;
    console.log("BUG_MANIFESTED 12");
  }
  return upstreamCalls >= 400 ? { status: 200, text: `summary of ${prompt}` } : { status: 529 };
}

async function completeWithRetry(prompt: string): Promise<string> {
  for (;;) {
    const res = await completePaid(prompt);
    if (res.status === 200 && res.text !== undefined) {
      return res.text;
    }
  }
}

void completeWithRetry("acme quarterly report");

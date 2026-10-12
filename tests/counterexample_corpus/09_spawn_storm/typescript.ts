// Bug: the agent fans out one summarize task per tenant document, with no bound on concurrency.
// TypeScript: tsc --strict accepts it; Promise.all starts every task at once and nothing limits the count.
const MAX_IN_FLIGHT = 8; // what the model gateway allows per tenant
let inFlight = 0;
let reported = false;

async function summarize(doc: string): Promise<number> {
  inFlight++;
  if (inFlight > MAX_IN_FLIGHT && !reported) {
    reported = true;
    console.log("BUG_MANIFESTED 09");
  }
  await new Promise((resolve) => setTimeout(resolve, 5)); // model call latency
  inFlight--;
  return 10 + (doc.length % 5);
}

async function summarizeFolder(docs: string[]): Promise<number> {
  const tokens = await Promise.all(docs.map((doc) => summarize(doc)));
  return tokens.reduce((sum, t) => sum + t, 0);
}

// acme's folder size comes from the tool call; nothing caps it.
const docs = Array.from({ length: 64 }, (_, i) => `acme/doc-${i}.md`);
void summarizeFolder(docs);

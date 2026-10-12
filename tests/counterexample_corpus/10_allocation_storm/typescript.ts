// Bug: the agent appends every search hit to its result buffer although the request wants the top 20.
// TypeScript: tsc --strict accepts it; the array grows until the process runs out of heap.
const MAX_BUFFERED = 4096; // per-request buffer budget the host would want (16 KiB of 32-bit ids)
let reported = false;

function searchHit(rank: number): number {
  return 700000 + rank; // record id of the hit at this rank
}

function collectHits(matched: number): number[] {
  const buffer: number[] = [];
  for (let rank = 0; rank < matched; rank++) {
    buffer.push(searchHit(rank));
    if (buffer.length > MAX_BUFFERED && !reported) {
      reported = true;
      console.log("BUG_MANIFESTED 10");
    }
  }
  return buffer;
}

const requested = 20;
// The search tool matched 20000 acme records and pages through all of them.
const topHits = collectHits(20000).slice(0, requested);

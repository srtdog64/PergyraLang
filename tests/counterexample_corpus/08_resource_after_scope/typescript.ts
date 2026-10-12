// Bug: the agent releases the acme search tool handle, then a retried tool call still uses it.
// TypeScript (--strict): accepted, no diagnostic. The handle is still in scope after close() and
// nothing checks its state, so the retry's line is silently never flushed.
class ToolHandle {
  private lines: string[] = [];

  constructor(readonly tenant: string, private readonly archive: string[]) {}

  record(line: string): void {
    this.lines.push(line);
  }

  close(): void {
    this.archive.push(...this.lines); // flush the transcript to storage
    this.lines = [];
  }
}

function retrySearch(handle: ToolHandle): void {
  handle.record("search retry: 1 hit");
}

const archive: string[] = [];
const handle = new ToolHandle("acme", archive);
try {
  handle.record("search: 3 hits");
} finally {
  handle.close(); // the agent ends the tool session
}
console.log("BUG_MANIFESTED 08"); // the stale use of the released handle follows
retrySearch(handle);

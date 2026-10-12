// Bug: one agent request runs waitForExport, which spins on the host clock until the job deadline.
// TypeScript: tsc --strict accepts it; nothing counts or limits host calls per request.
const EXPECTED_CALLS = 10; // a wait should need a handful of host calls
let reported = false;

function waitForExport(deadlineMs: number): void {
  let calls = 0;
  // Busy-wait: every spin is one host clock call.
  while (Date.now() < deadlineMs) {
    calls++;
    if (calls > EXPECTED_CALLS && !reported) {
      reported = true;
      console.log("BUG_MANIFESTED 11");
    }
  }
}

// Export job promised within 50 ms.
waitForExport(Date.now() + 50);

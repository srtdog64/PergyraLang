// Bug: the agent turn starts a background search tool call for tenant acme and returns without joining it.
// TypeScript (--strict): accepted, no diagnostic. A floating promise is legal; only the
// typescript-eslint rule no-floating-promises (not tsc) reports it.
type Session = { tenant: string; open: boolean; results: number[] };

const sleep = (ms: number) => new Promise<void>((resolve) => setTimeout(resolve, ms));

async function searchTool(session: Session): Promise<number> {
  await sleep(50); // the tool call outlives the turn
  if (!session.open) {
    // the result lands in a session the hub already closed
    console.log("BUG_MANIFESTED 05");
  }
  session.results.push(3);
  return 3;
}

async function draftReply(session: Session): Promise<string> {
  await sleep(10);
  return `reply for ${session.tenant}`;
}

async function runAgentTurn(session: Session): Promise<string> {
  searchTool(session); // started in the background, never awaited
  return await draftReply(session);
}

async function main(): Promise<void> {
  const session: Session = { tenant: "acme", open: true, results: [] };
  await runAgentTurn(session);
  session.open = false; // the hub closes the session once the turn reports completion
  await sleep(200); // the hub keeps serving other tenants
}

main();

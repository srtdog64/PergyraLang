// Bug: the user presses stop; the agent cancels the export tool call for tenant acme but never waits for it to finish.
// TypeScript (--strict): accepted, no diagnostic. abort() only signals; nothing requires
// awaiting the aborted promise, so a write already in flight lands after "stopped".
type Workspace = { tenant: string; exportStopped: boolean; files: string[] };

const sleep = (ms: number) => new Promise<void>((resolve) => setTimeout(resolve, ms));

async function writeChunk(ws: Workspace, chunk: number): Promise<void> {
  await sleep(40); // storage write, not abortable once started
  if (ws.exportStopped) {
    // the user was already told the export stopped
    console.log("BUG_MANIFESTED 06");
  }
  ws.files.push(`export-${chunk}.csv`);
}

async function exportTool(ws: Workspace, signal: AbortSignal): Promise<void> {
  for (const chunk of [1, 2, 3]) {
    if (signal.aborted) return;
    await writeChunk(ws, chunk);
  }
}

async function stopExport(ws: Workspace): Promise<void> {
  const controller = new AbortController();
  const exportCall = exportTool(ws, controller.signal);
  await sleep(10); // the user presses stop while chunk 1 is being written
  controller.abort();
  ws.exportStopped = true; // reported without awaiting exportCall
}

async function main(): Promise<void> {
  const ws: Workspace = { tenant: "acme", exportStopped: false, files: [] };
  await stopExport(ws);
  await sleep(200); // the hub keeps serving other tenants
}

main();

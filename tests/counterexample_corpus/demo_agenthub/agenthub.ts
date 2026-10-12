// AgentHub demo in TypeScript: the same agent turn as agenthub.pgy. Each mistake that
// demo.sh injects into the Pergyra file is one switch here; tsc --strict accepts all of
// them and every one runs. (No @types/node here, so `process` is declared locally.)
declare const process: { argv: string[] };

type Account = { tenant: string; credits: number };
type Ledger = { capacity: number; entries: number };
type ToolRun = { tool: string; planned: boolean; executed: number };

const mistake: string = process.argv[2] ?? "none";

async function searchTool(tenant: string): Promise<number> {
  await new Promise<void>((resolve) => setTimeout(resolve, 5));
  return tenant === "acme" ? 3 : 0;
}

function draftReply(tenant: string, hits: number): void {
  // The reply tool should only write; nothing stops it from reading other host state.
  if (mistake === "undeclared_capability") {
    const started = Date.now();
    console.log(`MISTAKE undeclared_capability ran: reply tool read the clock (${started > 0})`);
  }
  console.log(`reply to ${tenant}: ${hits} results`);
}

function runTool(name: string): number {
  const run: ToolRun = { tool: name, planned: false, executed: 0 };
  if (mistake !== "execute_before_plan") run.planned = true;
  run.executed += 1;
  if (!run.planned) console.log(`MISTAKE execute_before_plan ran: ${name} executed unplanned`);
  return run.executed;
}

// Charge, then record; on a record failure the charge should be undone.
function chargeTurn(account: Account, ledger: Ledger, cost: number): boolean {
  account.credits -= cost;
  if (ledger.capacity <= 0) {
    if (mistake !== "missing_compensation") account.credits += cost;
    else console.log(`MISTAKE missing_compensation ran: ${account.tenant} kept the charge (${account.credits})`);
    return false;
  }
  ledger.capacity -= 1;
  ledger.entries += 1;
  return true;
}

async function main(): Promise<void> {
  const acme: Account = { tenant: "acme", credits: 100 };
  const globex: Account = { tenant: "globex", credits: 500 };
  let hits = 0;
  if (mistake === "orphan_tool_call") {
    searchTool("acme").then((late) => console.log(`MISTAKE orphan_tool_call ran: ${late} results arrived after the reply`));
  } else {
    hits = await searchTool("acme");
  }
  const executed = runTool("summarize");
  draftReply("acme", hits);
  const ledger: Ledger = { capacity: 1, entries: 0 };
  const charged = mistake === "wrong_tenant" ? globex : acme;
  const first = chargeTurn(charged, ledger, 30);
  if (mistake === "wrong_tenant") console.log(`MISTAKE wrong_tenant ran: the acme agent charged globex (${globex.credits})`);
  console.log(`turn 1 charged=${first} acme=${acme.credits} globex=${globex.credits} executed=${executed}`);
  const second = chargeTurn(acme, ledger, 30);
  console.log(`turn 2 charged=${second} acme=${acme.credits} ledger=${ledger.entries}`);
}

main();

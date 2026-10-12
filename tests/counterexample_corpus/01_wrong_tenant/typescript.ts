// Bug: the agent acting for tenant acme runs a charge tool call against tenant globex's account.
// TypeScript (--strict): accepted, no diagnostic. The tenant is a field like any other; the
// lookup by account id never consults the session, so the cross-tenant debit runs.
type TenantId = "acme" | "globex";

interface Account {
  readonly id: string;
  readonly tenant: TenantId;
  credits: number;
}

interface AgentSession {
  readonly tenant: TenantId;
  readonly agentId: string;
}

const accounts = new Map<string, Account>([
  ["acct-1", { id: "acct-1", tenant: "acme", credits: 100 }],
  ["acct-2", { id: "acct-2", tenant: "globex", credits: 500 }],
]);

function chargeTool(session: AgentSession, accountId: string, amount: number): void {
  const account = accounts.get(accountId);
  if (account === undefined) {
    throw new Error(`unknown account ${accountId}`);
  }
  account.credits -= amount;
  if (account.tenant !== session.tenant) {
    console.log("BUG_MANIFESTED 01");
  }
}

// The model copied an account id from a shared document into its tool call.
const session: AgentSession = { tenant: "acme", agentId: "agent-7" };
chargeTool(session, "acct-2", 30);

// Bug: the agent's usage-charge tool debits acme's account without the account owner's approval.
// TypeScript (--strict): accepted, no diagnostic. The approval check is an ordinary function
// call; a tool that never calls it type-checks the same as one that does.
interface Account {
  readonly id: string;
  readonly ownerId: string;
  credits: number;
}

// Charges the owner has approved: account id -> approving user id.
const approvals = new Map<string, string>();

function requireOwnerApproval(account: Account): void {
  if (approvals.get(account.id) !== account.ownerId) {
    throw new Error(`charge on ${account.id} is not approved by its owner`);
  }
}

// The billing UI path checks the owner's approval before debiting.
export function chargeFromBillingUi(account: Account, amount: number): void {
  requireOwnerApproval(account);
  account.credits -= amount;
}

// The agent tool added later debits directly.
function chargeUsageTool(account: Account, amount: number): void {
  account.credits -= amount;
  if (approvals.get(account.id) !== account.ownerId) {
    console.log("BUG_MANIFESTED 02");
  }
}

const acme: Account = { id: "acct-1", ownerId: "acme-owner", credits: 100 };
chargeUsageTool(acme, 30);

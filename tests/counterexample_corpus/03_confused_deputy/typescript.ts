// Bug: a guest asks the agent (the deputy) to change acme's plan; the edit runs on the guest's say-so.
// TypeScript (--strict): accepted, no diagnostic. The store authorizes whichever principal is
// passed in, and the agent passes its own service principal instead of the requester's.
interface Principal {
  readonly id: string;
  readonly role: "owner" | "guest" | "service";
}

interface Account {
  readonly id: string;
  readonly ownerId: string;
  plan: number;
}

interface PlanRequest {
  readonly from: Principal;
  readonly accountId: string;
  readonly plan: number;
}

class AccountStore {
  constructor(private readonly accounts: Map<string, Account>) {}

  updatePlan(caller: Principal, accountId: string, plan: number): Account {
    const account = this.accounts.get(accountId);
    if (account === undefined) throw new Error(`unknown account ${accountId}`);
    if (caller.role !== "service" && caller.id !== account.ownerId) {
      throw new Error(`${caller.id} may not edit ${accountId}`);
    }
    account.plan = plan;
    return account;
  }
}

class AgentDeputy {
  constructor(private readonly self: Principal, private readonly store: AccountStore) {}

  handle(request: PlanRequest): void {
    const account = this.store.updatePlan(this.self, request.accountId, request.plan);
    if (request.from.id !== account.ownerId) {
      console.log("BUG_MANIFESTED 03");
    }
  }
}

const store = new AccountStore(new Map([["acct-1", { id: "acct-1", ownerId: "acme-owner", plan: 1 }]]));
const deputy = new AgentDeputy({ id: "agent-7", role: "service" }, store);
deputy.handle({ from: { id: "visitor-3", role: "guest" }, accountId: "acct-1", plan: 3 });

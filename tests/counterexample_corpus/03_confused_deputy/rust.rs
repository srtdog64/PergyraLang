// Bug: a guest asks the agent (the deputy) to change acme's plan; the edit runs on the guest's say-so.
// Rust: compiles with no warning and runs. The store trusts whichever principal is passed in; the agent passes its
// own service principal. An unforgeable EditGrant minted for the requester, with no service principal, would refuse it.
use std::collections::HashMap;

#[derive(Clone, Copy, PartialEq, Eq)]
enum Role { Guest, Service }

struct Principal {
    id: String,
    role: Role,
}

struct PlanRequest {
    from: Principal,
    account_id: String,
    plan: u32,
}

struct Account {
    owner_id: String,
    plan: u32,
}

struct AccountStore {
    accounts: HashMap<String, Account>,
}

impl AccountStore {
    fn update_plan(&mut self, caller: &Principal, account_id: &str, plan: u32) -> Result<&Account, String> {
        let Some(account) = self.accounts.get_mut(account_id) else {
            return Err(format!("unknown account {account_id}"));
        };
        if caller.role != Role::Service && caller.id != account.owner_id {
            return Err(format!("{} may not edit {account_id}", caller.id));
        }
        account.plan = plan;
        Ok(account)
    }
}

struct AgentDeputy {
    me: Principal,
    store: AccountStore,
}

impl AgentDeputy {
    fn handle(&mut self, request: &PlanRequest) -> Result<(), String> {
        let account = self.store.update_plan(&self.me, &request.account_id, request.plan)?;
        if request.from.id != account.owner_id {
            println!("BUG_MANIFESTED 03");
        }
        Ok(())
    }
}

fn main() -> Result<(), String> {
    let acme = Account { owner_id: "acme-owner".to_string(), plan: 1 };
    let store = AccountStore { accounts: HashMap::from([("acct-1".to_string(), acme)]) };
    let me = Principal { id: "agent-7".to_string(), role: Role::Service };
    let mut deputy = AgentDeputy { me, store };
    let guest = Principal { id: "visitor-3".to_string(), role: Role::Guest };
    deputy.handle(&PlanRequest { from: guest, account_id: "acct-1".to_string(), plan: 3 })
}

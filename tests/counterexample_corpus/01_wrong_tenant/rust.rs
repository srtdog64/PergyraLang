// Bug: the agent acting for tenant acme runs a charge tool call against tenant globex's account.
// Rust: compiles with no warning and runs. The tenant is a plain field the lookup by account id never consults.
// A tenant-typed id (AccountId<Acme> via PhantomData) would refuse it, but only for tenants known as types.
use std::collections::HashMap;

#[derive(Clone, Copy, PartialEq, Eq)]
enum Tenant {
    Acme,
    Globex,
}

struct Account {
    tenant: Tenant,
    credits: i64,
}

struct AgentSession {
    tenant: Tenant,
    agent_id: String,
}

fn charge_tool(
    accounts: &mut HashMap<String, Account>,
    session: &AgentSession,
    account_id: &str,
    amount: i64,
) -> Result<(), String> {
    let account = accounts
        .get_mut(account_id)
        .ok_or_else(|| format!("{}: unknown account {account_id}", session.agent_id))?;
    account.credits -= amount;
    if account.tenant != session.tenant {
        println!("BUG_MANIFESTED 01");
    }
    Ok(())
}

fn main() -> Result<(), String> {
    let mut accounts = HashMap::from([
        ("acct-1".to_string(), Account { tenant: Tenant::Acme, credits: 100 }),
        ("acct-2".to_string(), Account { tenant: Tenant::Globex, credits: 500 }),
    ]);
    let session = AgentSession { tenant: Tenant::Acme, agent_id: "agent-7".to_string() };
    // The model copied an account id from a shared document into its tool call.
    charge_tool(&mut accounts, &session, "acct-2", 30)
}

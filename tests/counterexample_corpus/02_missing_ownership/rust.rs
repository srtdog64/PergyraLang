// Bug: the agent's usage-charge tool debits acme's account without the account owner's approval.
// Rust: compiles with no warning and runs. The approval check is an ordinary call; a tool that never makes it
// compiles the same. A capability token (a private-field debit that takes an OwnerApproval) would refuse it.
use std::collections::HashMap;

pub struct Account {
    id: String,
    owner_id: String,
    credits: i64,
}

// approvals: account id -> approving user id.
pub struct Billing {
    approvals: HashMap<String, String>,
}

impl Billing {
    fn require_owner_approval(&self, account: &Account) -> Result<(), String> {
        match self.approvals.get(&account.id) {
            Some(by) if *by == account.owner_id => Ok(()),
            _ => Err(format!("charge on {} is not approved by its owner", account.id)),
        }
    }

    // The billing UI path checks the owner's approval before debiting (called by the UI, not shown).
    pub fn charge_from_billing_ui(&self, account: &mut Account, amount: i64) -> Result<(), String> {
        self.require_owner_approval(account)?;
        account.credits -= amount;
        Ok(())
    }

    // The agent tool added later debits directly.
    pub fn charge_usage_tool(&self, account: &mut Account, amount: i64) {
        account.credits -= amount;
        if self.approvals.get(&account.id) != Some(&account.owner_id) {
            println!("BUG_MANIFESTED 02");
        }
    }
}

fn main() {
    let billing = Billing { approvals: HashMap::new() };
    let mut acme = Account { id: "acct-1".to_string(), owner_id: "acme-owner".to_string(), credits: 100 };
    billing.charge_usage_tool(&mut acme, 30);
}

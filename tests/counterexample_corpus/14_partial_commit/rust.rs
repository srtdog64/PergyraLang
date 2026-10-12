// Bug: the agent's purchase tool debits the tenant wallet, then shipping fails.
// Rust: compiles without warnings. `?` returns early on the ship error and nothing undoes the
// debit: std has no transactions, and a refund-on-Drop guard is opt-in.
#[derive(Debug)]
enum PurchaseError {
    InsufficientFunds,
    OutOfStock,
}

struct Wallet {
    gold: i64, // acme's wallet
}

impl Wallet {
    fn debit(&mut self, amount: i64) -> Result<(), PurchaseError> {
        if self.gold < amount {
            return Err(PurchaseError::InsufficientFunds);
        }
        self.gold -= amount;
        Ok(())
    }
}

struct Warehouse {
    stock: u32,
    shipped: u32,
}

impl Warehouse {
    fn ship(&mut self) -> Result<(), PurchaseError> {
        if self.stock == 0 {
            return Err(PurchaseError::OutOfStock);
        }
        self.stock -= 1;
        self.shipped += 1;
        Ok(())
    }
}

// Tool call "purchase" run by the agent.
fn purchase_tool(wallet: &mut Wallet, warehouse: &mut Warehouse, price: i64) -> Result<(), PurchaseError> {
    wallet.debit(price)?;
    warehouse.ship()?;
    Ok(())
}

fn main() {
    let mut wallet = Wallet { gold: 100 };
    let mut warehouse = Warehouse { stock: 0, shipped: 0 };
    // The agent only sees true/false.
    let ok = purchase_tool(&mut wallet, &mut warehouse, 40).is_ok();
    if !ok && wallet.gold != 100 {
        println!("BUG_MANIFESTED 14");
    }
}

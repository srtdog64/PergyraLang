// Bug: the agent's purchase tool debits the tenant wallet, then shipping fails.
// TypeScript (--strict): accepted. Nothing ties the debit to an undo, so the
// failed purchase returns false and the debit stays applied.
class OutOfStockError extends Error {}

class Wallet {
  constructor(readonly tenant: string, public gold: number) {}

  debit(amount: number): void {
    this.gold -= amount;
  }

  refund(amount: number): void {
    this.gold += amount;
  }
}

class Warehouse {
  shipped = 0;

  constructor(public stock: number) {}

  ship(): void {
    if (this.stock <= 0) throw new OutOfStockError("out of stock");
    this.stock -= 1;
    this.shipped += 1;
  }
}

// Tool call "purchase" run by the agent; the agent only sees true/false.
function purchaseTool(wallet: Wallet, warehouse: Warehouse, price: number): boolean {
  try {
    wallet.debit(price);
    warehouse.ship();
    return true;
  } catch (e) {
    if (e instanceof OutOfStockError) return false;
    throw e;
  }
}

const wallet = new Wallet("acme", 100);
const ok = purchaseTool(wallet, new Warehouse(0), 40);
if (!ok && wallet.gold !== 100) {
  console.log("BUG_MANIFESTED 14");
}

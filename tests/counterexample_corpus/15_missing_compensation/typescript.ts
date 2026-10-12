// Bug: the purchase saga claims full rollback, but its debit step has no undo.
// TypeScript (--strict): accepted. `undo?` is optional, so a forgotten undo looks the
// same as an irreversible step; rollback skips the debit and the wallet stays debited.
type SagaStep = { name: string; run: () => void; undo?: () => void };

// Runs steps in order; on failure undoes the completed steps in reverse order.
function runSaga(steps: SagaStep[]): boolean {
  const done: SagaStep[] = [];
  for (const step of steps) {
    try {
      step.run();
      done.push(step);
    } catch {
      for (const completed of done.reverse()) completed.undo?.();
      return false;
    }
  }
  return true;
}

class Wallet {
  constructor(readonly tenant: string, public gold: number) {}
  debit(amount: number): void { this.gold -= amount; }
  refund(amount: number): void { this.gold += amount; }
}

class Warehouse {
  shipped = 0;
  constructor(public stock: number) {}
  ship(): void {
    if (this.stock <= 0) throw new Error("out of stock");
    this.stock -= 1;
    this.shipped += 1;
  }
}

const wallet = new Wallet("acme", 100);
const warehouse = new Warehouse(0);
let notifications = 0;
const ok = runSaga([
  { name: "debit", run: () => wallet.debit(40) },
  { name: "notify", run: () => { notifications += 1; } }, // cannot be recalled
  { name: "ship", run: () => warehouse.ship() },
]);
if (!ok && wallet.gold !== 100) {
  console.log("BUG_MANIFESTED 15");
}

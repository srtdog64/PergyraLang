// Bug: the agent's checkout tool captures the tenant's payment before authorizing it.
// TypeScript (--strict): accepted. Call order is not part of the Payment type, so
// capture() on a pending payment type-checks and runs.
type PaymentStatus = "pending" | "authorized" | "captured";

class Payment {
  status: PaymentStatus = "pending";
  captured = 0;

  constructor(readonly tenant: string, readonly amount: number) {}

  authorize(): void {
    this.status = "authorized";
  }

  capture(): void {
    this.captured += this.amount;
    this.status = "captured";
  }
}

// Tool call "checkout" run by the agent for tenant acme.
function runCheckoutTool(tenant: string, amount: number): number {
  const payment = new Payment(tenant, amount);
  const statusAtCapture = payment.status;
  payment.capture();
  if (statusAtCapture !== "authorized") {
    console.log("BUG_MANIFESTED 13");
  }
  payment.authorize();
  return payment.captured;
}

runCheckoutTool("acme", 40);

// Bug: the agent's checkout tool captures the tenant's payment before authorizing it.
// Rust: compiles without warnings. The status is a runtime enum field, so capture() on a pending
// payment compiles; a typestate design (authorize(self) -> Payment<Authorized>) would refuse it.
#[derive(Clone, Copy, PartialEq, Eq)]
enum PaymentStatus {
    Pending,
    Authorized,
    Captured,
}

struct Payment {
    tenant: String,
    amount: i64,
    status: PaymentStatus,
    captured: i64,
}

impl Payment {
    fn new(tenant: &str, amount: i64) -> Self {
        Payment { tenant: tenant.to_string(), amount, status: PaymentStatus::Pending, captured: 0 }
    }

    fn authorize(&mut self) {
        self.status = PaymentStatus::Authorized;
    }

    fn capture(&mut self) {
        self.captured += self.amount;
        self.status = PaymentStatus::Captured;
    }
}

// Tool call "checkout" run by the agent for tenant acme; the agent gets the receipt text.
fn run_checkout_tool(tenant: &str, amount: i64) -> String {
    let mut payment = Payment::new(tenant, amount);
    let status_at_capture = payment.status;
    payment.capture();
    if status_at_capture != PaymentStatus::Authorized {
        println!("BUG_MANIFESTED 13");
    }
    payment.authorize();
    format!("{} captured={}", payment.tenant, payment.captured)
}

fn main() {
    run_checkout_tool("acme", 40);
}

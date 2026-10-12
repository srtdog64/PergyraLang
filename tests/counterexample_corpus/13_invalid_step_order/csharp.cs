// Bug: the agent's checkout tool captures the tenant's payment before authorizing it.
// C#: compiles with no warning. Call order is not part of the Payment type, so
// Capture() on a pending payment compiles and runs.

// Tool call "checkout" run by the agent for tenant acme.
static int RunCheckoutTool(string tenant, int amount)
{
    var payment = new Payment(tenant, amount);
    var statusAtCapture = payment.Status;
    payment.Capture();
    if (statusAtCapture != PaymentStatus.Authorized)
    {
        Console.WriteLine("BUG_MANIFESTED 13");
    }
    payment.Authorize();
    return payment.Captured;
}

RunCheckoutTool("acme", 40);

enum PaymentStatus { Pending, Authorized, Captured }

sealed class Payment(string tenant, int amount)
{
    public string Tenant { get; } = tenant;
    public int Amount { get; } = amount;
    public PaymentStatus Status { get; private set; } = PaymentStatus.Pending;
    public int Captured { get; private set; }

    public void Authorize() => Status = PaymentStatus.Authorized;

    public void Capture()
    {
        Captured += Amount;
        Status = PaymentStatus.Captured;
    }
}

// Bug: the purchase saga claims full rollback, but its debit step has no undo.
// C#: compiles with no warning. `Action? Undo` is nullable, so a forgotten undo looks
// the same as an irreversible step; rollback skips the debit and the wallet stays debited.

// Runs steps in order; on failure undoes the completed steps in reverse order.
static bool RunSaga(IReadOnlyList<SagaStep> steps)
{
    var done = new Stack<SagaStep>();
    foreach (var step in steps)
    {
        try
        {
            step.Run();
            done.Push(step);
        }
        catch (Exception)
        {
            while (done.TryPop(out var completed)) completed.Undo?.Invoke();
            return false;
        }
    }
    return true;
}

var wallet = new Wallet("acme", 100);
var warehouse = new Warehouse(0);
var notifications = 0;
var ok = RunSaga([
    new SagaStep("debit", () => wallet.Debit(40)),
    new SagaStep("notify", () => notifications++), // cannot be recalled
    new SagaStep("ship", warehouse.Ship),
]);
if (!ok && wallet.Gold != 100)
{
    Console.WriteLine("BUG_MANIFESTED 15");
}

sealed record SagaStep(string Name, Action Run, Action? Undo = null);

sealed class Wallet(string tenant, int gold)
{
    public string Tenant { get; } = tenant;
    public int Gold { get; private set; } = gold;
    public void Debit(int amount) => Gold -= amount;
    public void Refund(int amount) => Gold += amount;
}

sealed class Warehouse(int stock)
{
    public int Stock { get; private set; } = stock;
    public int Shipped { get; private set; }

    public void Ship()
    {
        if (Stock <= 0) throw new InvalidOperationException("out of stock");
        Stock -= 1;
        Shipped += 1;
    }
}

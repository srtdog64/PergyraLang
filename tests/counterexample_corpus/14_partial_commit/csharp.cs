// Bug: the agent's purchase tool debits the tenant wallet, then shipping fails.
// C#: compiles with no warning. Nothing ties the debit to an undo, so the failed
// purchase returns false and the debit stays applied.

// Tool call "purchase" run by the agent; the agent only sees true/false.
static bool PurchaseTool(Wallet wallet, Warehouse warehouse, int price)
{
    try
    {
        wallet.Debit(price);
        warehouse.Ship();
        return true;
    }
    catch (OutOfStockException)
    {
        return false;
    }
}

var wallet = new Wallet("acme", 100);
var ok = PurchaseTool(wallet, new Warehouse(0), 40);
if (!ok && wallet.Gold != 100)
{
    Console.WriteLine("BUG_MANIFESTED 14");
}

sealed class OutOfStockException(string message) : Exception(message);

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
        if (Stock <= 0) throw new OutOfStockException("out of stock");
        Stock -= 1;
        Shipped += 1;
    }
}

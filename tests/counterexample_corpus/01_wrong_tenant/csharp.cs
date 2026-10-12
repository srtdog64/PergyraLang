// Bug: the agent acting for tenant acme runs a charge tool call against tenant globex's account.
// C#: compiles with no warning and runs. The tenant is an ordinary property; the lookup by
// account id never consults the session, so the cross-tenant debit runs.
var accounts = new Dictionary<string, Account>
{
    ["acct-1"] = new Account("acct-1", Tenant.Acme, 100),
    ["acct-2"] = new Account("acct-2", Tenant.Globex, 500),
};

// The model copied an account id from a shared document into its tool call.
var session = new AgentSession(Tenant.Acme, "agent-7");
new ChargeTool(accounts).Run(session, "acct-2", 30);

enum Tenant { Acme, Globex }

sealed record AgentSession(Tenant Tenant, string AgentId);

sealed class Account(string id, Tenant tenant, int credits)
{
    public string Id { get; } = id;
    public Tenant Tenant { get; } = tenant;
    public int Credits { get; set; } = credits;
}

sealed class ChargeTool(IReadOnlyDictionary<string, Account> accounts)
{
    public void Run(AgentSession session, string accountId, int amount)
    {
        if (!accounts.TryGetValue(accountId, out var account))
        {
            throw new KeyNotFoundException($"unknown account {accountId}");
        }
        account.Credits -= amount;
        if (account.Tenant != session.Tenant)
        {
            Console.WriteLine("BUG_MANIFESTED 01");
        }
    }
}

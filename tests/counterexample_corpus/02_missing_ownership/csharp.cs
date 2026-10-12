// Bug: the agent's usage-charge tool debits acme's account without the account owner's approval.
// C#: compiles with no warning and runs. The approval check is an ordinary method call; a tool
// that never calls it compiles the same as one that does.
var approvals = new Dictionary<string, string>();
var billing = new Billing(approvals);
var acme = new Account("acct-1", "acme-owner", 100);
billing.ChargeUsageTool(acme, 30);

sealed class Account(string id, string ownerId, int credits)
{
    public string Id { get; } = id;
    public string OwnerId { get; } = ownerId;
    public int Credits { get; set; } = credits;
}

// approvals: account id -> approving user id.
sealed class Billing(IReadOnlyDictionary<string, string> approvals)
{
    void RequireOwnerApproval(Account account)
    {
        if (!approvals.TryGetValue(account.Id, out var by) || by != account.OwnerId)
        {
            throw new UnauthorizedAccessException($"charge on {account.Id} is not approved by its owner");
        }
    }

    // The billing UI path checks the owner's approval before debiting.
    public void ChargeFromBillingUi(Account account, int amount)
    {
        RequireOwnerApproval(account);
        account.Credits -= amount;
    }

    // The agent tool added later debits directly.
    public void ChargeUsageTool(Account account, int amount)
    {
        account.Credits -= amount;
        if (!approvals.TryGetValue(account.Id, out var by) || by != account.OwnerId)
        {
            Console.WriteLine("BUG_MANIFESTED 02");
        }
    }
}

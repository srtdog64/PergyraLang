// Bug: a guest asks the agent (the deputy) to change acme's plan; the edit runs on the guest's say-so.
// C#: compiles with no warning and runs. The store authorizes whichever principal is passed in,
// and the agent passes its own service principal instead of the requester's.
var store = new AccountStore(new Dictionary<string, Account>
{
    ["acct-1"] = new Account("acct-1", "acme-owner", 1),
});
var deputy = new AgentDeputy(new Principal("agent-7", Role.Service), store);
deputy.Handle(new PlanRequest(new Principal("visitor-3", Role.Guest), "acct-1", 3));

enum Role { Owner, Guest, Service }

sealed record Principal(string Id, Role Role);

sealed record PlanRequest(Principal From, string AccountId, int Plan);

sealed class Account(string id, string ownerId, int plan)
{
    public string Id { get; } = id;
    public string OwnerId { get; } = ownerId;
    public int Plan { get; set; } = plan;
}

sealed class AccountStore(Dictionary<string, Account> accounts)
{
    public Account UpdatePlan(Principal caller, string accountId, int plan)
    {
        if (!accounts.TryGetValue(accountId, out var account))
            throw new KeyNotFoundException($"unknown account {accountId}");
        if (caller.Role != Role.Service && caller.Id != account.OwnerId)
            throw new UnauthorizedAccessException($"{caller.Id} may not edit {accountId}");
        account.Plan = plan;
        return account;
    }
}

sealed class AgentDeputy(Principal self, AccountStore store)
{
    public void Handle(PlanRequest request)
    {
        var account = store.UpdatePlan(self, request.AccountId, request.Plan);
        if (request.From.Id != account.OwnerId)
            Console.WriteLine("BUG_MANIFESTED 03");
    }
}

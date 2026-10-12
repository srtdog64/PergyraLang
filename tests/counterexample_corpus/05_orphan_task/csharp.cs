// Bug: the agent turn starts a background search tool call for tenant acme and returns without joining it.
// C#: compiles with warning CS4014 (call is not awaited inside an async method) and runs;
// the task keeps running after the turn returns.
var session = new Session("acme");
await RunAgentTurnAsync(session);
session.Open = false; // the hub closes the session once the turn reports completion
await Task.Delay(200); // the hub keeps serving other tenants

static async Task<int> SearchToolAsync(Session session)
{
    await Task.Delay(50); // the tool call outlives the turn
    if (!session.Open)
    {
        // the result lands in a session the hub already closed
        Console.WriteLine("BUG_MANIFESTED 05");
    }
    session.Results.Add(3);
    return 3;
}

static async Task<string> DraftReplyAsync(Session session)
{
    await Task.Delay(10);
    return $"reply for {session.Tenant}";
}

static async Task<string> RunAgentTurnAsync(Session session)
{
    SearchToolAsync(session); // started in the background, never awaited
    return await DraftReplyAsync(session);
}

sealed class Session(string tenant)
{
    public string Tenant { get; } = tenant;
    public bool Open { get; set; } = true;
    public List<int> Results { get; } = [];
}

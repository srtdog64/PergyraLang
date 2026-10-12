// Bug: the support agent's turn is granted io_write (post the reply) only, but a context tool reads a local file.
// C#: compiles with no warning and runs. The grant is runtime data the tool never consults; any code
// can call System.IO, and .NET (Core and later) has no in-process permission model.
var workspace = Directory.CreateTempSubdirectory("agenthub-").FullName;
File.WriteAllText(Path.Combine(workspace, "admin_notes.txt"), "globex renewal at risk");
var turn = new TurnContext("acme", new HashSet<Capability> { Capability.IoWrite }, workspace, new List<string>());
Agent.RunAgentTurn(turn, "acme-42 reset password");
Directory.Delete(workspace, recursive: true);

enum Capability { IoRead, IoWrite, Network }

sealed record TurnContext(string Tenant, IReadOnlySet<Capability> Caps, string Workspace, List<string> Outbox);

static class Agent
{
    static string AttachContext(TurnContext ctx, string ticket)
    {
        var notes = File.ReadAllText(Path.Combine(ctx.Workspace, "admin_notes.txt"));
        if (!ctx.Caps.Contains(Capability.IoRead))
        {
            Console.WriteLine("BUG_MANIFESTED 04");
        }
        return $"{ticket} | {notes}";
    }

    public static void RunAgentTurn(TurnContext ctx, string ticket)
    {
        if (!ctx.Caps.Contains(Capability.IoWrite))
        {
            throw new InvalidOperationException("this turn may not post a reply");
        }
        ctx.Outbox.Add($"Re: {AttachContext(ctx, ticket)}");
    }
}

// Bug: the agent keeps a handle to the world's tool zone, the world rotates to the
// next tenant, and the agent then records a call through the stale handle.
// C#: compiles with no warning. A get-only property returns a reference that can outlive the rotation.
var world = new AgentWorld(new ToolZone(new Session("acme")));
var handle = world.Tools;
world.Rotate("globex");
handle.Session.Record();
if (!ReferenceEquals(handle, world.Tools) && handle.Session.Calls == 1)
{
    Console.WriteLine("BUG_MANIFESTED 16");
}

sealed class Session(string tenant)
{
    public string Tenant { get; } = tenant;
    public int Calls { get; private set; }
    public void Record() => Calls += 1;
}

sealed class ToolZone(Session session)
{
    public Session Session { get; } = session;
}

sealed class AgentWorld(ToolZone tools)
{
    public ToolZone Tools { get; private set; } = tools;
    public void Rotate(string tenant) => Tools = new ToolZone(new Session(tenant));
}

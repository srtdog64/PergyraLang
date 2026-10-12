// Bug: the agent releases the acme search tool handle, then a retried tool call still uses it.
// C#: compiles with no warning. The handle wraps a StringWriter that checks its own state, so the
// stale use throws ObjectDisposedException at runtime; the hub logs the failed call to stderr.
var archive = new List<string>();
var handle = new ToolHandle("acme", archive);
try
{
    handle.Record("search: 3 hits");
}
finally
{
    handle.Dispose(); // the agent ends the tool session
}

try
{
    Console.WriteLine("BUG_MANIFESTED 08"); // the stale use of the released handle follows
    RetrySearch(handle);
}
catch (Exception e)
{
    Console.Error.WriteLine($"tool call failed: {e.GetType().Name}: {e.Message}");
}

static void RetrySearch(ToolHandle handle) => handle.Record("search retry: 1 hit");

sealed class ToolHandle(string tenant, List<string> archive) : IDisposable
{
    private readonly StringWriter transcript = new();
    public string Tenant { get; } = tenant;

    public void Record(string line) => transcript.WriteLine(line);

    public void Dispose()
    {
        archive.Add(transcript.ToString()); // flush the transcript to storage
        transcript.Dispose();
    }
}

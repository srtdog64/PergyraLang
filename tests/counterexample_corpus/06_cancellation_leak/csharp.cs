// Bug: the user presses stop; the agent cancels the export tool call for tenant acme but never waits for it to finish.
// C#: compiles with no warning (the task is stored, so CS4014 does not apply). Cancel() only
// signals the token; a write already in flight lands after "stopped".
var workspace = new Workspace("acme");
await StopExportAsync(workspace);
await Task.Delay(200); // the hub keeps serving other tenants

static async Task WriteChunkAsync(Workspace ws, int chunk)
{
    await Task.Delay(40); // storage write, not cancellable once started
    if (ws.ExportStopped)
    {
        // the user was already told the export stopped
        Console.WriteLine("BUG_MANIFESTED 06");
    }
    ws.Files.Add($"export-{chunk}.csv");
}

static async Task ExportToolAsync(Workspace ws, CancellationToken token)
{
    foreach (var chunk in new[] { 1, 2, 3 })
    {
        if (token.IsCancellationRequested) return;
        await WriteChunkAsync(ws, chunk);
    }
}

static async Task StopExportAsync(Workspace ws)
{
    using var cts = new CancellationTokenSource();
    var exportCall = ExportToolAsync(ws, cts.Token);
    await Task.Delay(10); // the user presses stop while chunk 1 is being written
    cts.Cancel();
    ws.ExportStopped = true; // reported without awaiting exportCall
}

sealed class Workspace(string tenant)
{
    public string Tenant { get; } = tenant;
    public bool ExportStopped { get; set; }
    public List<string> Files { get; } = [];
}

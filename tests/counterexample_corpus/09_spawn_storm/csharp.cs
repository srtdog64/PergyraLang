// Bug: the agent fans out one summarize task per tenant document, with no bound on concurrency.
// C#: compiles without warnings; Task.WhenAll over Select starts every task at once.
const int MaxInFlight = 8; // what the model gateway allows per tenant
int inFlight = 0;
int reported = 0;

async Task<int> SummarizeAsync(string doc)
{
    if (Interlocked.Increment(ref inFlight) > MaxInFlight && Interlocked.Exchange(ref reported, 1) == 0)
    {
        Console.WriteLine("BUG_MANIFESTED 09");
    }
    await Task.Delay(5); // model call latency
    Interlocked.Decrement(ref inFlight);
    return 10 + doc.Length % 5;
}

async Task<int> SummarizeFolderAsync(IEnumerable<string> docs)
{
    int[] tokens = await Task.WhenAll(docs.Select(SummarizeAsync));
    return tokens.Sum();
}

// acme's folder size comes from the tool call; nothing caps it.
var docs = Enumerable.Range(0, 64).Select(i => $"acme/doc-{i}.md");
await SummarizeFolderAsync(docs);

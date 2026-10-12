// Bug: the agent retries a paid model call until it succeeds; every attempt is billed.
// C#: compiles without warnings; nothing bounds attempts or spend.
const int PriceCents = 3;
const int RequestBudgetCents = 10;
int upstreamCalls = 0;
int spentCents = 0;
bool reported = false;

// Local stand-in for the paid API (no network): overloaded (529) for the first 400 calls.
Task<Completion> CompletePaidAsync(string prompt)
{
    upstreamCalls++;
    spentCents += PriceCents;
    if (spentCents > RequestBudgetCents && !reported)
    {
        reported = true;
        Console.WriteLine("BUG_MANIFESTED 12");
    }
    return Task.FromResult(upstreamCalls >= 400 ? new Completion(200, $"summary of {prompt}") : new Completion(529, null));
}

async Task<string> CompleteWithRetryAsync(string prompt)
{
    while (true)
    {
        var res = await CompletePaidAsync(prompt);
        if (res.Status == 200 && res.Text is not null)
        {
            return res.Text;
        }
    }
}

await CompleteWithRetryAsync("acme quarterly report");

record Completion(int Status, string? Text);

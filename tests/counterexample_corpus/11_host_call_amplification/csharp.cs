// Bug: one agent request runs WaitForExport, which spins on the host clock until the job deadline.
// C#: compiles without warnings; nothing counts or limits host calls per request.
const int ExpectedCalls = 10; // a wait should need a handful of host calls
bool reported = false;

void WaitForExport(DateTime deadline)
{
    int calls = 0;
    // Busy-wait: every spin is one host clock call.
    while (DateTime.UtcNow < deadline)
    {
        calls++;
        if (calls > ExpectedCalls && !reported)
        {
            reported = true;
            Console.WriteLine("BUG_MANIFESTED 11");
        }
    }
}

// Export job promised within 50 ms.
WaitForExport(DateTime.UtcNow.AddMilliseconds(50));

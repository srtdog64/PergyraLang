// Bug: the agent appends every search hit to its result buffer although the request wants the top 20.
// C#: compiles without warnings; List<int> grows until the process runs out of memory.
const int MaxBuffered = 4096; // per-request buffer budget the host would want (16 KiB of 32-bit ids)
bool reported = false;

int SearchHit(int rank) => 700000 + rank; // record id of the hit at this rank

List<int> CollectHits(int matched)
{
    var buffer = new List<int>();
    for (int rank = 0; rank < matched; rank++)
    {
        buffer.Add(SearchHit(rank));
        if (buffer.Count > MaxBuffered && !reported)
        {
            reported = true;
            Console.WriteLine("BUG_MANIFESTED 10");
        }
    }
    return buffer;
}

const int Requested = 20;
// The search tool matched 20000 acme records and pages through all of them.
var top = CollectHits(20000).Take(Requested).ToList();

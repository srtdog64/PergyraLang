// Bug: the planner waits for the executor's capacity while the executor waits for the plan first; neither proceeds.
// C#: compiles with no warning. Both ReadAsync calls wait forever and nothing detects the
// cycle; only the turn deadline (Task.WhenAny with a delay) observes the hang.
using System.Threading.Channels;

var planCh = Channel.CreateBounded<int>(1);
var capacityCh = Channel.CreateBounded<int>(1);

var planner = Task.Run(async () =>
{
    var capacity = await capacityCh.Reader.ReadAsync();
    await planCh.Writer.WriteAsync(capacity);
});
var executor = Task.Run(async () =>
{
    var plan = await planCh.Reader.ReadAsync(); // waits for the plan first
    await capacityCh.Writer.WriteAsync(3); // and only then announces its capacity
    return plan;
});

var turn = Task.WhenAll(planner, executor);
if (await Task.WhenAny(turn, Task.Delay(2000)) != turn) // turn deadline
{
    Console.WriteLine("BUG_MANIFESTED 07");
    return;
}
Console.WriteLine($"acme turn executed {await executor} steps");

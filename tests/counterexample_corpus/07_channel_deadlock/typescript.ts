// Bug: the planner waits for the executor's capacity while the executor waits for the plan first; neither proceeds.
// TypeScript (--strict): accepted, no diagnostic. Both awaits stay pending; without the turn
// deadline timer Node would drain its event loop and exit 0 with the turn never finished.
class Channel<T> {
  private items: T[] = [];
  private waiters: ((value: T) => void)[] = [];

  send(value: T): void {
    const waiter = this.waiters.shift();
    if (waiter) waiter(value);
    else this.items.push(value);
  }

  recv(): Promise<T> {
    if (this.items.length > 0) return Promise.resolve(this.items.shift() as T);
    return new Promise<T>((resolve) => this.waiters.push(resolve));
  }
}

async function planner(planCh: Channel<number>, capacityCh: Channel<number>): Promise<void> {
  const capacity = await capacityCh.recv();
  planCh.send(capacity);
}

async function executor(planCh: Channel<number>, capacityCh: Channel<number>): Promise<number> {
  const plan = await planCh.recv(); // waits for the plan first
  capacityCh.send(3); // and only then announces its capacity
  return plan;
}

async function runAgentTurn(): Promise<number> {
  const planCh = new Channel<number>();
  const capacityCh = new Channel<number>();
  const [, steps] = await Promise.all([planner(planCh, capacityCh), executor(planCh, capacityCh)]);
  return steps;
}

const deadline = setTimeout(() => console.log("BUG_MANIFESTED 07"), 2000); // turn deadline
runAgentTurn().then((steps) => {
  clearTimeout(deadline);
  console.log(`acme turn executed ${steps} steps`);
});

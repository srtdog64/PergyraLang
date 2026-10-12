// Bug: the agent keeps a handle to the world's tool zone, the world rotates to the
// next tenant, and the agent then records a call through the stale handle.
// TypeScript (--strict): accepted. A getter returns a plain reference that can outlive the rotation.
class Session {
  calls = 0;
  constructor(readonly tenant: string) {}
  record(): void {
    this.calls += 1;
  }
}

class ToolZone {
  constructor(readonly session: Session) {}
}

class AgentWorld {
  #tools: ToolZone;

  constructor(tools: ToolZone) {
    this.#tools = tools;
  }

  get tools(): ToolZone {
    return this.#tools;
  }

  rotate(tenant: string): void {
    this.#tools = new ToolZone(new Session(tenant));
  }
}

const world = new AgentWorld(new ToolZone(new Session("acme")));
const handle = world.tools;
world.rotate("globex");
handle.session.record();
if (handle !== world.tools && handle.session.calls === 1) {
  console.log("BUG_MANIFESTED 16");
}

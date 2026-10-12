// Bug: the agent keeps a handle to the world's tool zone, the world rotates to the
// next tenant, and the agent then records a call through the stale handle.
// Rust: refused at compile time, E0499: the &mut borrow of the zone is still live when rotate() takes &mut world.
struct Session {
    tenant: String,
    calls: u32,
}

impl Session {
    fn new(tenant: &str) -> Self {
        Session { tenant: tenant.to_string(), calls: 0 }
    }

    fn record(&mut self) {
        self.calls += 1;
    }
}

struct ToolZone {
    session: Session,
}

struct AgentWorld {
    tools: ToolZone,
}

impl AgentWorld {
    fn tools_mut(&mut self) -> &mut ToolZone {
        &mut self.tools
    }

    fn rotate(&mut self, tenant: &str) {
        self.tools = ToolZone { session: Session::new(tenant) };
    }

    fn tenant(&self) -> &str {
        &self.tools.session.tenant
    }
}

fn main() {
    let mut world = AgentWorld { tools: ToolZone { session: Session::new("acme") } };
    let handle = world.tools_mut();
    world.rotate("globex");
    handle.session.record();
    // acme's retired session got the call while the world already serves globex.
    if handle.session.tenant == "acme" && handle.session.calls == 1 && world.tenant() == "globex" {
        println!("BUG_MANIFESTED 16");
    }
}

// Bug: the agent releases the acme search tool handle, then a retried tool call still uses it.
// Rust: rejected at compile time, E0382 (borrow of moved value: `handle`): close(self) consumes the handle.
// If the handle were kept as Option<ToolHandle> in a long-lived session, the stale use would compile (runtime None).
struct ToolHandle<'a> {
    tenant: String,
    archive: &'a mut Vec<String>,
    transcript: String,
}

impl<'a> ToolHandle<'a> {
    fn open(tenant: &str, archive: &'a mut Vec<String>) -> Self {
        ToolHandle { tenant: tenant.to_string(), archive, transcript: String::new() }
    }

    fn record(&mut self, line: &str) {
        self.transcript.push_str(line);
        self.transcript.push('\n');
    }

    // The agent ends the tool session: flush the transcript to storage and give the handle up.
    fn close(self) {
        self.archive.push(format!("{}: {}", self.tenant, self.transcript));
    }
}

fn retry_search(handle: &mut ToolHandle) {
    handle.record("search retry: 1 hit");
}

fn main() {
    let mut archive = Vec::new();
    let mut handle = ToolHandle::open("acme", &mut archive);
    handle.record("search: 3 hits");
    handle.close();
    println!("BUG_MANIFESTED 08"); // the stale use of the released handle follows
    retry_search(&mut handle);
}

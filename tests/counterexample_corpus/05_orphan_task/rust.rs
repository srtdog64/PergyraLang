// Bug: the agent turn starts a background search tool call for tenant acme and returns without joining it.
// Rust: compiles with no warning: thread::spawn and JoinHandle are not #[must_use], and dropping the handle
// detaches the thread. A borrowed &Session is refused (E0521), so the state is shared by Arc; thread::scope would join.
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

struct Session {
    tenant: String,
    open: AtomicBool,
    results: Mutex<Vec<u32>>,
}

fn search_tool(session: &Session) -> u32 {
    thread::sleep(Duration::from_millis(50)); // the tool call outlives the turn
    if !session.open.load(Ordering::SeqCst) {
        // the result lands in a session the hub already closed
        println!("BUG_MANIFESTED 05");
    }
    session.results.lock().unwrap().push(3);
    3
}

fn draft_reply(session: &Session) -> String {
    thread::sleep(Duration::from_millis(10));
    format!("reply for {}", session.tenant)
}

fn run_agent_turn(session: &Arc<Session>) -> String {
    let background = Arc::clone(session);
    thread::spawn(move || search_tool(&background)); // started in the background, never joined
    draft_reply(session)
}

fn main() {
    let session = Arc::new(Session {
        tenant: "acme".to_string(),
        open: AtomicBool::new(true),
        results: Mutex::new(Vec::new()),
    });
    run_agent_turn(&session);
    session.open.store(false, Ordering::SeqCst); // the hub closes the session once the turn reports completion
    thread::sleep(Duration::from_millis(200)); // the hub keeps serving other tenants
}

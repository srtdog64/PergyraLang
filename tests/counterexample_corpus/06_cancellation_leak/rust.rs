// Bug: the user presses stop; the agent cancels the export tool call for tenant acme but never waits for it to finish.
// Rust: compiles with one warning, unused_variables on the never-joined handle (JoinHandle is not #[must_use]).
// The cancel flag only signals, so an in-flight write lands after "stopped"; thread::scope joins only at scope end.
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

struct Workspace {
    tenant: String,
    export_stopped: AtomicBool,
    files: Mutex<Vec<String>>,
}

fn write_chunk(ws: &Workspace, chunk: u32) {
    thread::sleep(Duration::from_millis(40)); // storage write, not cancellable once started
    if ws.export_stopped.load(Ordering::SeqCst) {
        // the user was already told the export stopped
        println!("BUG_MANIFESTED 06");
    }
    ws.files.lock().unwrap().push(format!("{}/export-{chunk}.csv", ws.tenant));
}

fn export_tool(ws: &Workspace, cancel: &AtomicBool) {
    for chunk in 1..=3 {
        if cancel.load(Ordering::SeqCst) {
            return;
        }
        write_chunk(ws, chunk);
    }
}

fn stop_export(ws: &Arc<Workspace>) {
    let cancel = Arc::new(AtomicBool::new(false));
    let export_call = {
        let (ws, cancel) = (Arc::clone(ws), Arc::clone(&cancel));
        thread::spawn(move || export_tool(&ws, &cancel))
    };
    thread::sleep(Duration::from_millis(10)); // the user presses stop while chunk 1 is being written
    cancel.store(true, Ordering::SeqCst);
    ws.export_stopped.store(true, Ordering::SeqCst); // reported without joining export_call
}

fn main() {
    let ws = Arc::new(Workspace {
        tenant: "acme".to_string(),
        export_stopped: AtomicBool::new(false),
        files: Mutex::new(Vec::new()),
    });
    stop_export(&ws);
    thread::sleep(Duration::from_millis(200)); // the hub keeps serving other tenants
}

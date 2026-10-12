// Bug: the agent fans out one summarize task per tenant document, with no bound on concurrency.
// Rust: compiles without warnings; one scoped std thread per document starts every task at once
// and nothing limits the count (a bounded worker pool or semaphore is opt-in).
use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use std::thread;
use std::time::Duration;

const MAX_IN_FLIGHT: usize = 8; // what the model gateway allows per tenant
static IN_FLIGHT: AtomicUsize = AtomicUsize::new(0);
static REPORTED: AtomicBool = AtomicBool::new(false);

fn summarize(doc: &str) -> usize {
    if IN_FLIGHT.fetch_add(1, Ordering::SeqCst) + 1 > MAX_IN_FLIGHT && !REPORTED.swap(true, Ordering::SeqCst) {
        println!("BUG_MANIFESTED 09");
    }
    thread::sleep(Duration::from_millis(5)); // model call latency
    IN_FLIGHT.fetch_sub(1, Ordering::SeqCst);
    10 + doc.len() % 5
}

fn summarize_folder(docs: &[String]) -> usize {
    thread::scope(|s| {
        let tasks: Vec<_> = docs.iter().map(|doc| s.spawn(move || summarize(doc))).collect();
        tasks.into_iter().map(|t| t.join().unwrap()).sum()
    })
}

fn main() {
    // acme's folder size comes from the tool call; nothing caps it.
    let docs: Vec<String> = (0..64).map(|i| format!("acme/doc-{i}.md")).collect();
    summarize_folder(&docs);
}

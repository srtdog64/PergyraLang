// Bug: one agent request runs wait_for_export, which spins on the host clock until the job deadline.
// Rust: compiles without warnings; nothing counts or limits host calls per request.
use std::time::{Duration, Instant};

const EXPECTED_CALLS: u32 = 10; // a wait should need a handful of host calls

fn wait_for_export(deadline: Instant) {
    let mut calls = 0;
    let mut reported = false;
    // Busy-wait: every spin is one host clock call.
    while Instant::now() < deadline {
        calls += 1;
        if calls > EXPECTED_CALLS && !reported {
            reported = true;
            println!("BUG_MANIFESTED 11");
        }
    }
}

fn main() {
    // Export job promised within 50 ms.
    wait_for_export(Instant::now() + Duration::from_millis(50));
}

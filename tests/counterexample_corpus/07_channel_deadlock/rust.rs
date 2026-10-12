// Bug: the planner waits for the executor's capacity while the executor waits for the plan first; neither proceeds.
// Rust: compiles with no warning. std::sync::mpsc does not detect the wait cycle: both recv() calls block
// forever, and only the turn deadline (recv_timeout on the result channel) observes the hang.
use std::sync::mpsc::{self, RecvTimeoutError};
use std::thread;
use std::time::Duration;

fn main() {
    let (plan_tx, plan_rx) = mpsc::sync_channel::<u32>(1);
    let (capacity_tx, capacity_rx) = mpsc::sync_channel::<u32>(1);
    let (steps_tx, steps_rx) = mpsc::channel::<u32>();

    // planner
    thread::spawn(move || {
        let capacity = capacity_rx.recv().unwrap();
        plan_tx.send(capacity).unwrap();
    });
    // executor
    thread::spawn(move || {
        let plan = plan_rx.recv().unwrap(); // waits for the plan first
        capacity_tx.send(3).unwrap(); // and only then announces its capacity
        steps_tx.send(plan).unwrap();
    });

    match steps_rx.recv_timeout(Duration::from_secs(2)) {
        // turn deadline
        Ok(steps) => println!("acme turn executed {steps} steps"),
        Err(RecvTimeoutError::Timeout) => println!("BUG_MANIFESTED 07"),
        Err(RecvTimeoutError::Disconnected) => eprintln!("executor ended without reporting steps"),
    }
}

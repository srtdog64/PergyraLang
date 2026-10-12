// Bug: the purchase saga claims full rollback, but its debit step has no undo.
// Rust: compiles without warnings. `undo: None` means both "forgot the undo" and "irreversible",
// so rollback skips the debit and the wallet stays debited.
struct Purchase {
    gold: i64, // acme's wallet
    stock: u32,
    shipped: u32,
    notifications: u32,
}

struct SagaStep {
    name: &'static str,
    run: fn(&mut Purchase) -> Result<(), String>,
    undo: Option<fn(&mut Purchase)>,
}

// Runs steps in order; on failure undoes the completed steps in reverse order.
fn run_saga(state: &mut Purchase, steps: &[SagaStep]) -> Result<(), String> {
    for (i, step) in steps.iter().enumerate() {
        if let Err(e) = (step.run)(state) {
            for completed in steps[..i].iter().rev() {
                if let Some(undo) = completed.undo {
                    undo(state);
                }
            }
            return Err(format!("{}: {e}", step.name));
        }
    }
    Ok(())
}

fn main() {
    let mut state = Purchase { gold: 100, stock: 0, shipped: 0, notifications: 0 };
    let steps = [
        SagaStep { name: "debit", run: |p| { p.gold -= 40; Ok(()) }, undo: None },
        SagaStep { name: "notify", run: |p| { p.notifications += 1; Ok(()) }, undo: None }, // cannot be recalled
        SagaStep {
            name: "ship",
            run: |p| {
                if p.stock == 0 {
                    return Err("out of stock".to_string());
                }
                p.stock -= 1;
                p.shipped += 1;
                Ok(())
            },
            undo: None,
        },
    ];
    let ok = run_saga(&mut state, &steps).is_ok();
    if !ok && state.gold != 100 {
        println!("BUG_MANIFESTED 15");
    }
}

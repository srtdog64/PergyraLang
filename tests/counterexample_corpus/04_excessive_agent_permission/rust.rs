// Bug: the support agent's turn is granted io_write (post the reply) only, but a context tool reads a local file.
// Rust: compiles with no warning and runs. The grant is runtime data the tool never consults; any code can call
// std::fs (ambient authority). Rust has no in-process capability model; clippy's disallowed_methods is an opt-in lint.
use std::collections::HashSet;
use std::fs;
use std::io;
use std::path::PathBuf;

#[derive(Clone, Copy, PartialEq, Eq, Hash)]
enum Capability {
    IoRead,
    IoWrite,
}

struct TurnContext {
    caps: HashSet<Capability>,
    workspace: PathBuf,
    outbox: Vec<String>,
}

fn attach_context(ctx: &TurnContext, ticket: &str) -> io::Result<String> {
    let notes = fs::read_to_string(ctx.workspace.join("admin_notes.txt"))?;
    if !ctx.caps.contains(&Capability::IoRead) {
        println!("BUG_MANIFESTED 04");
    }
    Ok(format!("{ticket} | {notes}"))
}

fn run_agent_turn(ctx: &mut TurnContext, ticket: &str) -> io::Result<()> {
    if !ctx.caps.contains(&Capability::IoWrite) {
        return Err(io::Error::new(io::ErrorKind::PermissionDenied, "this turn may not post a reply"));
    }
    let reply = format!("Re: {}", attach_context(ctx, ticket)?);
    ctx.outbox.push(reply);
    Ok(())
}

fn main() -> io::Result<()> {
    // Host setup: the operator's notes file sits in the agent's workspace.
    let workspace = std::env::temp_dir().join(format!("agenthub-{}", std::process::id()));
    fs::create_dir_all(&workspace)?;
    fs::write(workspace.join("admin_notes.txt"), "globex renewal at risk")?;
    let caps = HashSet::from([Capability::IoWrite]);
    let mut turn = TurnContext { caps, workspace: workspace.clone(), outbox: Vec::new() };
    run_agent_turn(&mut turn, "acme-42 reset password")?;
    fs::remove_dir_all(&workspace)
}

// Bug: the agent appends every search hit to its result buffer although the request wants the top 20.
// Rust: compiles without warnings; the Vec grows with the hit count, and if allocation ever fails
// the process aborts (Vec::try_reserve is opt-in; std has no per-request allocation budget).
const MAX_BUFFERED: usize = 4096; // per-request buffer budget the host would want (16 KiB of 32-bit ids)

fn search_hit(rank: u32) -> u32 {
    700000 + rank // record id of the hit at this rank
}

fn collect_hits(matched: u32) -> Vec<u32> {
    let mut reported = false;
    let mut buffer = Vec::new();
    for rank in 0..matched {
        buffer.push(search_hit(rank));
        if buffer.len() > MAX_BUFFERED && !reported {
            reported = true;
            println!("BUG_MANIFESTED 10");
        }
    }
    buffer
}

fn main() {
    let requested = 20;
    // The search tool matched 20000 acme records and pages through all of them.
    let mut top_hits = collect_hits(20000);
    top_hits.truncate(requested);
}

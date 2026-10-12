// Bug: the agent retries a paid model call until it succeeds; every attempt is billed.
// Rust: compiles without warnings. The Result is inspected, so #[must_use] is satisfied, and
// nothing bounds attempts or spend.
const PRICE_CENTS: u32 = 3;
const REQUEST_BUDGET_CENTS: u32 = 10;

enum ApiError {
    Overloaded, // HTTP 529
}

// Local stand-in for the paid API (no network): overloaded (529) for the first 400 calls.
struct PaidModelApi {
    upstream_calls: u32,
    spent_cents: u32,
    reported: bool,
}

impl PaidModelApi {
    fn complete(&mut self, prompt: &str) -> Result<String, ApiError> {
        self.upstream_calls += 1;
        self.spent_cents += PRICE_CENTS;
        if self.spent_cents > REQUEST_BUDGET_CENTS && !self.reported {
            self.reported = true;
            println!("BUG_MANIFESTED 12");
        }
        if self.upstream_calls >= 400 {
            return Ok(format!("summary of {prompt}"));
        }
        Err(ApiError::Overloaded)
    }
}

fn complete_with_retry(api: &mut PaidModelApi, prompt: &str) -> String {
    loop {
        match api.complete(prompt) {
            Ok(text) => return text,
            Err(ApiError::Overloaded) => continue,
        }
    }
}

fn main() {
    let mut api = PaidModelApi { upstream_calls: 0, spent_cents: 0, reported: false };
    complete_with_retry(&mut api, "acme quarterly report");
}

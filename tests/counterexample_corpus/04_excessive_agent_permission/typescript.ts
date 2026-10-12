// Bug: the support agent's turn is granted io_write (post the reply) only, but a context tool reads a local file.
// TypeScript (--strict): accepted, no diagnostic. The grant is runtime data the tool never consults;
// any module can call fs. Node's opt-in --permission flag restricts fs per process, at run time.

// Node built-ins. @types/node is not installed in this environment, so the calls used are declared here.
declare function require(id: "node:fs"): {
  mkdtempSync(prefix: string): string;
  writeFileSync(file: string, data: string): void;
  readFileSync(file: string, encoding: "utf8"): string;
  rmSync(file: string, options: { recursive: boolean; force: boolean }): void;
};
declare function require(id: "node:os"): { tmpdir(): string };
declare function require(id: "node:path"): { join(...parts: string[]): string };

const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");

type Capability = "io_read" | "io_write" | "network";

interface TurnContext {
  readonly tenant: string;
  readonly caps: ReadonlySet<Capability>;
  readonly workspace: string;
  readonly outbox: string[];
}

function attachContext(ctx: TurnContext, ticket: string): string {
  const notes = fs.readFileSync(path.join(ctx.workspace, "admin_notes.txt"), "utf8");
  if (!ctx.caps.has("io_read")) {
    console.log("BUG_MANIFESTED 04");
  }
  return `${ticket} | ${notes}`;
}

function runAgentTurn(ctx: TurnContext, ticket: string): void {
  if (!ctx.caps.has("io_write")) {
    throw new Error("this turn may not post a reply");
  }
  ctx.outbox.push(`Re: ${attachContext(ctx, ticket)}`);
}

// Host setup: the operator's notes file sits in the agent's workspace.
const workspace = fs.mkdtempSync(path.join(os.tmpdir(), "agenthub-"));
fs.writeFileSync(path.join(workspace, "admin_notes.txt"), "globex renewal at risk");
const turn: TurnContext = { tenant: "acme", caps: new Set<Capability>(["io_write"]), workspace, outbox: [] };
runAgentTurn(turn, "acme-42 reset password");
fs.rmSync(workspace, { recursive: true, force: true });

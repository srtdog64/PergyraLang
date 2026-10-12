# Bug: the agent acting for tenant acme runs a charge tool call against tenant globex's account.
# Python: runs with no warning. The tenant is an ordinary attribute; the lookup by account id
# never consults the session, so the cross-tenant debit runs.
from dataclasses import dataclass
from typing import Literal

TenantId = Literal["acme", "globex"]


@dataclass
class Account:
    id: str
    tenant: TenantId
    credits: int


@dataclass(frozen=True)
class AgentSession:
    tenant: TenantId
    agent_id: str


accounts: dict[str, Account] = {
    "acct-1": Account("acct-1", "acme", 100),
    "acct-2": Account("acct-2", "globex", 500),
}


def charge_tool(session: AgentSession, account_id: str, amount: int) -> None:
    account = accounts.get(account_id)
    if account is None:
        raise KeyError(f"unknown account {account_id}")
    account.credits -= amount
    if account.tenant != session.tenant:
        print("BUG_MANIFESTED 01")


# The model copied an account id from a shared document into its tool call.
session = AgentSession(tenant="acme", agent_id="agent-7")
charge_tool(session, "acct-2", 30)

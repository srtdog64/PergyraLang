# Bug: a guest asks the agent (the deputy) to change acme's plan; the edit runs on the guest's say-so.
# Python: runs with no warning. The store authorizes whichever principal is passed in, and the
# agent passes its own service principal instead of the requester's.
from dataclasses import dataclass
from typing import Literal


@dataclass(frozen=True)
class Principal:
    id: str
    role: Literal["owner", "guest", "service"]


@dataclass
class Account:
    id: str
    owner_id: str
    plan: int


@dataclass(frozen=True)
class PlanRequest:
    sender: Principal
    account_id: str
    plan: int


class AccountStore:
    def __init__(self, accounts: dict[str, Account]) -> None:
        self._accounts = accounts

    def update_plan(self, caller: Principal, account_id: str, plan: int) -> Account:
        account = self._accounts[account_id]
        if caller.role != "service" and caller.id != account.owner_id:
            raise PermissionError(f"{caller.id} may not edit {account_id}")
        account.plan = plan
        return account


class AgentDeputy:
    def __init__(self, me: Principal, store: AccountStore) -> None:
        self._me = me
        self._store = store

    def handle(self, request: PlanRequest) -> None:
        account = self._store.update_plan(self._me, request.account_id, request.plan)
        if request.sender.id != account.owner_id:
            print("BUG_MANIFESTED 03")


store = AccountStore({"acct-1": Account("acct-1", "acme-owner", 1)})
deputy = AgentDeputy(Principal("agent-7", "service"), store)
deputy.handle(PlanRequest(Principal("visitor-3", "guest"), "acct-1", 3))

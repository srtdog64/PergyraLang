# Bug: the agent's usage-charge tool debits acme's account without the account owner's approval.
# Python: runs with no warning. The approval check is an ordinary function call; a tool that
# never calls it runs the same as one that does.
from dataclasses import dataclass


@dataclass
class Account:
    id: str
    owner_id: str
    credits: int


# Charges the owner has approved: account id -> approving user id.
approvals: dict[str, str] = {}


def require_owner_approval(account: Account) -> None:
    if approvals.get(account.id) != account.owner_id:
        raise PermissionError(f"charge on {account.id} is not approved by its owner")


def charge_from_billing_ui(account: Account, amount: int) -> None:
    """The billing UI path checks the owner's approval before debiting."""
    require_owner_approval(account)
    account.credits -= amount


def charge_usage_tool(account: Account, amount: int) -> None:
    """The agent tool added later debits directly."""
    account.credits -= amount
    if approvals.get(account.id) != account.owner_id:
        print("BUG_MANIFESTED 02")


acme = Account(id="acct-1", owner_id="acme-owner", credits=100)
charge_usage_tool(acme, 30)

# Bug: the agent's checkout tool captures the tenant's payment before authorizing it.
# Python: runs without complaint. Call order is not part of the Payment type, so
# capture() on a pending payment executes.
from dataclasses import dataclass
from enum import Enum


class PaymentStatus(Enum):
    PENDING = "pending"
    AUTHORIZED = "authorized"
    CAPTURED = "captured"


@dataclass
class Payment:
    tenant: str
    amount: int
    status: PaymentStatus = PaymentStatus.PENDING
    captured: int = 0

    def authorize(self) -> None:
        self.status = PaymentStatus.AUTHORIZED

    def capture(self) -> None:
        self.captured += self.amount
        self.status = PaymentStatus.CAPTURED


def run_checkout_tool(tenant: str, amount: int) -> int:
    """Tool call "checkout" run by the agent for tenant acme."""
    payment = Payment(tenant, amount)
    status_at_capture = payment.status
    payment.capture()
    if status_at_capture is not PaymentStatus.AUTHORIZED:
        print("BUG_MANIFESTED 13")
    payment.authorize()
    return payment.captured


run_checkout_tool("acme", 40)

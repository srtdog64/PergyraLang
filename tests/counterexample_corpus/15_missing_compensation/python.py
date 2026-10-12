# Bug: the purchase saga claims full rollback, but its debit step has no undo.
# Python: runs without complaint. `undo` defaults to None, so a forgotten undo looks the
# same as an irreversible step; rollback skips the debit and the wallet stays debited.
from collections.abc import Callable
from dataclasses import dataclass


@dataclass(frozen=True)
class SagaStep:
    name: str
    run: Callable[[], None]
    undo: Callable[[], None] | None = None


def run_saga(steps: list[SagaStep]) -> bool:
    """Runs steps in order; on failure undoes the completed steps in reverse order."""
    done: list[SagaStep] = []
    for step in steps:
        try:
            step.run()
        except Exception:
            for completed in reversed(done):
                if completed.undo is not None:
                    completed.undo()
            return False
        done.append(step)
    return True


@dataclass
class Wallet:
    tenant: str
    gold: int

    def debit(self, amount: int) -> None:
        self.gold -= amount

    def refund(self, amount: int) -> None:
        self.gold += amount


@dataclass
class Warehouse:
    stock: int
    shipped: int = 0

    def ship(self) -> None:
        if self.stock <= 0:
            raise RuntimeError("out of stock")
        self.stock -= 1
        self.shipped += 1


wallet = Wallet("acme", 100)
warehouse = Warehouse(0)
notifications: list[str] = []
ok = run_saga([
    SagaStep("debit", lambda: wallet.debit(40)),
    SagaStep("notify", lambda: notifications.append("acme: order placed")),  # cannot be recalled
    SagaStep("ship", warehouse.ship),
])
if not ok and wallet.gold != 100:
    print("BUG_MANIFESTED 15")

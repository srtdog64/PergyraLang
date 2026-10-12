# Bug: the agent's purchase tool debits the tenant wallet, then shipping fails.
# Python: runs without complaint. Nothing ties the debit to an undo, so the failed
# purchase returns False and the debit stays applied.
from dataclasses import dataclass


class OutOfStockError(Exception):
    pass


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
            raise OutOfStockError("out of stock")
        self.stock -= 1
        self.shipped += 1


def purchase_tool(wallet: Wallet, warehouse: Warehouse, price: int) -> bool:
    """Tool call "purchase" run by the agent; the agent only sees True/False."""
    try:
        wallet.debit(price)
        warehouse.ship()
        return True
    except OutOfStockError:
        return False


wallet = Wallet("acme", 100)
ok = purchase_tool(wallet, Warehouse(0), 40)
if not ok and wallet.gold != 100:
    print("BUG_MANIFESTED 14")

**Intent**
`shopping_mall_checkout_refund` validates the current `page != zone` rule with a commerce flow:

- pages are projection surfaces
- zones are execution/authority boundaries
- intents move the buyer through cart, checkout, refund, and account sync

**Layout**
- `intents/commerce_intents.pgy`: checkout, refund, and account sync intents
- `subjects/`: `Member`, `Merchant`, payment/product classes, and vessels
- `zones/`: cart, payment, refund, account
- `pages/surfaces.pgy`: page/API projections and render helpers
- `world.pgy`: transcript orchestration
- `setup.pgy`: factory-style world construction

**What It Proves**
- `transfer: cart -> payment` and `transfer: refund -> account` work as concrete zone-instance bindings
- page rendering can stay outside zone types while still consuming zone-owned state
- `IntentHistory*` and trace output are usable for user-facing simulation reports
- `page != zone` can still produce a coherent JS/backend style transcript

**Current Notes**
- page models are plain `object`/`dto` projections, not a new language primitive
- zones still own the business transition logic
- the transcript is JS/backend flavored, but the real execution boundary remains the zone/intent layer
- while building this example, the misleading intent warning for explicit `on:` steps was fixed in the compiler
- `using:` + actor materialization is strong enough for routing/trace, but nested actor state is still safer when a canonical actor is updated explicitly and zones mirror shared execution state

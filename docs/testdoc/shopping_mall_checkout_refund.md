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
- `api/commerce_api.pgy`: request/response dto and intent adapter handlers
- `report/transcript_report.pgy`: transcript/report composition and file writer
- `world.pgy`: transcript orchestration
- `setup.pgy`: factory-style world construction

**What It Proves**
- `transfer: cart -> payment` and `transfer: refund -> account` work as concrete zone-instance bindings
- page rendering can stay outside zone types while still consuming zone-owned state
- API-style request handling is an adapter layer, not the execution boundary itself
- request dto -> adapter handler -> intent -> response/report dto is a stable shape for JS/backend style apps
- `IntentHistory*` and trace output are usable for user-facing simulation reports
- `page != zone` can still produce a coherent JS/backend style transcript

**Current Notes**
- page models are plain `object`/`dto` projections, not a new language primitive
- API calls in the transcript should be read as `request dto -> intent adapter -> zone/world execution -> response dto`
- zones still own the business transition logic
- the transcript is JS/backend flavored, but the real execution boundary remains the zone/intent layer
- while building this example, the misleading intent warning for explicit `on:` steps was fixed in the compiler
- `using:` now rebinds `who` actors to live zone subject slots during the step body, so zone methods can mutate nested actor state directly without the old "canonical actor first, zone mirrors later" workaround

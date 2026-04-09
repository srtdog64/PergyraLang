# composite_intent_orchestration

`composite_intent_orchestration` is a focused intent-container example.

It demonstrates:
- child intents with their own `where / using / who / compensate`
- a parent intent that orchestrates child intents through `step { intent: ... }`
- a second-level parent intent that treats the first parent as a reusable container
- runtime observability through `IntentLast*` and `IntentHistory*`
- nested intents sharing the same subject without tripping the active-intent
  conflict scheduler

Structure:
- `ReserveStock`
- `ChargeBuyer`
- `ShipParcel`
- `FulfillOrder`
- `ProcessOrder`

The important point is that `FulfillOrder` and `ProcessOrder` do not directly own
zone contracts. They act as orchestration containers, while the leaf intents keep
the concrete zone/participant semantics.

Expected runtime shape:
- `ProcessOrder=true`
- canonical and zone-local clerk state all reach `1/1/1`
- history contains only the orchestration step at the top level, while the child
  intents succeed underneath it

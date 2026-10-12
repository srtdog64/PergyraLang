# Bug: one agent request runs wait_for_export, which spins on the host clock until the job deadline.
# Python: accepted; nothing counts or limits host calls per request.
import time

EXPECTED_CALLS = 10  # a wait should need a handful of host calls
reported = False


def wait_for_export(deadline: float) -> None:
    global reported
    calls = 0
    # Busy-wait: every spin is one host clock call.
    while time.monotonic() < deadline:
        calls += 1
        if calls > EXPECTED_CALLS and not reported:
            reported = True
            print("BUG_MANIFESTED 11")


# Export job promised within 50 ms.
wait_for_export(time.monotonic() + 0.05)

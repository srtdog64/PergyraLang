"""Behavior and allocation limits for the imported MIR row-owner probe."""
from pathlib import Path
import json
import re
import sys

work = Path(sys.argv[1])
costs = []
for count in [256, 512]:
    expected = f'{count}{count}{count*(count-1)//2}'
    assert (work / f'{count}.out').read_text().replace('\r', '') == expected, 'row content drift'
    measured = re.fullmatch(r'row_allocation_bytes=(\d+)\n',
                            (work / f'{count}.err').read_text())
    assert measured, 'missing or unexpected allocation observation'
    costs.append(int(measured.group(1)))
assert 0 < costs[0] <= costs[1] <= 4 * 1024 * 1024, f'row allocation budget exceeded: {costs}'
assert costs[1] <= costs[0] * 2.6, f'nonlinear row allocation: {costs}'
result = {'rows': [256, 512], 'requested_bytes': costs,
          'doubling_ratio': costs[1] / costs[0]}
(work / 'cost.json').write_text(json.dumps(result, indent=2) + '\n')
print('[mir-row-mutation-cost] PASS', json.dumps(result), work)

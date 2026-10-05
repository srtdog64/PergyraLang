# 길드 경제 한 시즌

길드원 넷이 한 시즌 동안 원정에서 금화와 재료를 벌고, 유물을 만들고, 경매로 사고판다.
금화는 왕실 조폐국이 발행하고 소각한다. 매 intent가 끝날 때마다 **보존 법칙**을 검사한다.

```text
treasury.supply == 지갑 합 + 경매장 escrow + 대장간 feeBox + 국고 vault + 지급 대기분
```

## intent 목차 (먼저 읽을 것)

| intent | 현실 목적 | 성공하면 | 거절되는 곳 |
|---|---|---|---|
| [`ClearRaid`](domains/raid/intents/clear_raid.pgy) | 원정대가 보스를 쓰러뜨리고 보상을 나눈다 | 보상 금화가 새로 발행되어(faucet) 기여도대로 남김없이 지급된다 | 피해 부족·면허 없음은 `pre:engage`. 시즌 발행 한도 초과는 `pre:mint`이고, 이미 기록된 처치는 보상으로 되돌린다 |
| [`CraftRelic`](domains/forge/intents/craft_relic.pgy) | 대장장이가 재료로 유물을 만든다 | 재료와 수수료가 빠지고 유물이 생긴다. 수수료는 소각된다(sink) | 면허·재료·수수료·빈 칸 중 하나라도 모자라면 `pre:smelt` |
| [`SettleAuction`](domains/market/intents/settle_auction.pgy) | 낙찰자가 값을 치르고 유물을 받는다 | 낙찰가 → escrow → 세금은 국고, 나머지는 판매자 | 판매자에게 유물이 없거나 구매자가 못 내면 `pre:open` |

## 폴더 구조

```text
game_economy_demo/
  main.pgy                 시즌 줄거리
  genesis.pgy              시즌 시작 상태
  world.pgy                다섯 zone을 묶는 실행 경계, 보존 법칙, 시나리오
  expected_stdout.txt      두 백엔드의 기대 출력
  commons/                 도메인과 무관한 것
    types/                 Calling(직업), MemberCard(공개 카드)
    ranking.pgy            결정적 집계와 동점 규칙(앞선 인덱스 우선)
  domains/                 바운디드 컨텍스트마다 하나
    guild/                 정체성: 길드원, 면허, 명부
    treasury/              통화 공급: 조폐국, 국고, 발행 한도
    raid/                  원정 (intent·zone·정산 규칙)
    forge/                 제작 (intent·zone·품질과 수수료 규칙)
    market/                경매 (intent·zone·효과·관계·경매 규칙)
```

`docs/34_intent_oriented_paradigm.md` §10.1의 도메인 분할을 따른다. 도메인마다
`intents/`, `zones/`, `subjects/`, `abilities/`, `effects/`, `relations/`를 필요한 만큼만 두고,
순수 규칙은 `policy.pgy` 한 파일에 둔다.

이전 구조는 `subject/`, `zone/`, `intent/`, `effect/`처럼 **키워드 종류**로 폴더를 나눴다.
그러면 "무엇으로 만들었나"가 먼저 보이고 "무엇을 하려는가"는 안 보인다. `util/`과 `async/`는
책임이 없는 버킷이었다. 하위 폴더 파일들이 루트 기준 경로로 import해서, 경로가 import하는
파일 기준으로 풀리는 이 언어에서는(`docs/109`) 처음부터 컴파일되지 않았다.

## 경계를 고른 이유

- **subject는 정체성과 권한이다.** `Adventurer`의 금화(`Purse`)와 물건(`Pack`)은 vessel에만
  있다. 금화를 꺼내거나 유물을 넘기는 action은 `authorized by self`다. 금화를 만들고 없애는
  능력 `Minting`은 `Mint`에만 구현된다.
- **zone은 실제 자원 경계다.** 명부(원본 정체성), 국고(통화 공급), 원정 집결지, 대장간
  (`feeBox`), 경매장(`escrow`). 권한을 선언한 zone의 step에는 모두 `authorized by:`가 필요하다.
- **원본은 명부에만 있다.** 다른 zone의 자리는 처음엔 복제다. step이 `using:`으로 원본을 다시
  묶고, step이 끝나면 변경이 원본으로 돌아온다.
- **world는 밖으로 새지 않는다.** zone을 world 밖으로 꺼낼 수 없어서, intent 호출은 모두
  world 메서드 안에 있다.
- **계산은 policy, 결정은 intent.** 면허 표, 누진 경매세, 감정가, 2차 가격 경매, 원정 분배,
  제작 품질은 순수 함수다. world는 그 결과(plan)를 값으로 intent에 넘기고, 성공·실패와
  그 이유(`IntentLastFailure()`)는 intent가 소유한다.
- **병렬은 결정적 집계에만 쓴다.** 피해량 수집(`join with all`), 합계(`sum`), 최고·최저
  (`max`/`min`), 동점 중 앞선 인덱스(`min`)는 모두 `parallel join`이다. join은 인덱스 순서로
  고정된 fold라 워커 수와 무관하다. intent 절 안에는 동시성 구문을 두지 않는다.

## 경제 규칙

- 발행(faucet): 원정 보상. 보스 체력 × 5/3. 시즌 발행 한도는 200.
- 소각(sink): 대장간 수수료 `12 + heat/5`. 화로는 제작마다 25도 오른다.
- 세금: 누진 경매세(100 미만 5%, 300 미만 8%, 그 이상 12%). 국고에 쌓이고 공급량은 그대로다.
- 경매: 2차 가격(Vickrey). 최고 입찰자가 낙찰받고 차순위 입찰가(최저가 이상)를 낸다.
- 원정 분배: 피해 비례. 나눗셈 나머지는 MVP에게 간다. 몫의 합은 언제나 보상과 같다.

## 실행

저장소 루트에서:

```bash
PGY_NATIVE_PIPELINE=1 bin/pgy examples/game_economy_demo/main.pgy --run --backend=c
```

`--backend=llvm`도 같은 출력을 낸다. `PGY_WORKERS=1/2/4/8`에서도 출력이 바이트 단위로 같다.

## 측정으로 확인한 언어 동작 (2026-10-05, `d0fa49ea` 빌드)

- step 절의 평가 순서: `pre → invariant → on → guard → expect → post → invariant`.
  `guard`는 동작을 막는 사전 조건이 아니라 사후 검사다. 동작을 막으려면 `pre:`를 쓴다.
- `pre:`는 step마다 하나다. 여러 조건은 `&&`로 묶는다.
- `on:`이 없는 step은 같은 이름의 action 계약을 찾고, 없으면 경고를 낸다.
- subject는 함수가 값으로 돌려줄 수 없다(zone 자리에 묶인 핸들).
- `roster`, `pool`은 예약어다.

## 알려진 언어 결함 (이 예제가 피해 가는 것)

1. **보상이 원본 참가자에 닿지 않는다.** step이 `using:`으로 빌린 참가자를 보상(compensate)이
   건드리면, 그 변경은 zone의 복제본에 들어가고 원본에는 돌아오지 않는다. 진단도 없다.
   그래서 이 예제는 실패할 수 있는 검사를 모두 첫 효과 앞에 두고, 보상은 zone이 소유한
   상태(처치 기록·발행·escrow 등)에서만 실제로 일어나게 했다.
2. **world 메서드는 subject 인자를 받을 수 없다.** 의미 검사는 통과하지만 C(인자 형 불일치)와
   LLVM(verify 실패) 모두 잘못된 호출을 만든다. 그래서 world는 `Member` 값으로 자리를 고르고,
   판매자×구매자 표로 intent를 부른다.
3. **`type Gold = Int` 같은 별칭이 MIR/LLVM에서 깨진다.** C는 루프 phi 형 충돌, LLVM은
   `Result<Gold, E>`의 별칭을 못 푼다. 그래서 금화는 그냥 `Int`다.
4. **LLVM은 zone shared 배열 원소를 바꿀 수 없다.** 그래서 잔액은 배열 장부가 아니라
   길드원의 vessel에 있다.

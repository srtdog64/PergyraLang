# Pergyra Diagnostic Vocabulary

마지막 업데이트: 2026-04-20

진단 메시지에 등장하는 ownership/타입 용어의 **canonical set** 정의 및 drift 매핑.

---

## 왜 이 문서가 필요한가

코드베이스 grep 결과 (2026-04-20):

| 용어 | 등장 횟수 |
|---|---|
| `subject` | 166 |
| `boundary value` | 27 |
| `anchored handle` | 21 |
| `movable resource` | 15 |
| `capability-bearing` | 7 |
| `move token` | 1 |

문제:
- "anchored handle" vs "anchored resource handle" 혼용
- 같은 의미가 메시지마다 다른 라벨 (예: `boundary value` vs `boundary-tracked value`)
- 사용자는 이 용어 chain (Slot → SecureSlot → DeviceSlot → QubitSlot → anchored handle → movable handle → ...)를 매번 추적해야 함

→ 진단 메시지에서 사용할 **canonical 4개 용어**를 고정하고, 세부 분류는 괄호로만 노출한다.

---

## Canonical Vocabulary (4 terms)

### 1. `slot handle`

owning 자원 핸들의 통칭. Slot/SecureSlot/DeviceSlot/QubitSlot 등 **anchored 또는 movable 한 모든 자원 핸들**을 가리킬 때 사용.

세부 분류 (필요할 때만 괄호):
- `slot handle (anchored)` — Slot/SecureSlot/DeviceSlot. local-only, 채널/return 경계 통과 불가
- `slot handle (movable)` — QubitSlot 류. own/move 경계 통과 가능
- `slot handle (capability-bearing)` — SecureSlot/Token. authority 컨텍스트 필요

> **Migration**: `anchored handle` → `slot handle (anchored)`, `anchored resource handle` → `slot handle (anchored)`, `movable resource` → `slot handle (movable)`, `movable resource handle` → `slot handle (movable)`

### 2. `subject`

identity-bearing active host. zone/world에 anchored된 도메인 객체. 세부 분류 없이 단독으로 사용.

> **Migration**: `subject host` → `subject host slot` (host 슬롯이 명확할 때만), 그 외는 `subject` 유지

### 3. `boundary value`

own/ref 함수 경계를 통과하는 값. ownership transfer 또는 borrow tracking이 적용되는 모든 비-copy 타입의 통칭.

세부 분류:
- `boundary value (subject)` — subject 가 경계로 들어옴
- `boundary value (slot handle)` — slot 핸들이 경계로 들어옴
- `boundary value (borrow-tracked)` — provenance가 추적되는 일반 값

> **Migration**: `boundary value` 유지. 단 `boundary-tracked value` → `boundary value (borrow-tracked)`

### 4. `capability token`

권한을 운반하는 token. SecureSlot의 paired token + Token<T> 통칭.

> **Migration**: `capability-bearing value` → `capability token` (Token에 한정될 때) 또는 `slot handle (capability-bearing)` (SecureSlot 자체일 때). `move token` → `capability token (move)`

---

## Drift 매핑표

진단 메시지 작성 시 좌측 → 우측으로 변환:

| 기존 표현 | Canonical |
|---|---|
| anchored resource handle | slot handle (anchored) |
| anchored handle | slot handle (anchored) |
| movable resource handle | slot handle (movable) |
| movable resource | slot handle (movable) |
| capability-bearing value | slot handle (capability-bearing) 또는 capability token |
| capability-bearing values | slot handles (capability-bearing) |
| move token | capability token (move) |
| boundary-tracked value | boundary value (borrow-tracked) |
| Slot/SecureSlot/DeviceSlot | slot handle (anchored) — 세부 타입 명시 필요 시 그대로 |
| QubitSlot | slot handle (movable) — 구체 타입 명시 필요 시 그대로 |

---

## 사용 규칙

1. **첫 등장은 canonical 우선**. 진단 메시지의 첫 번째 명사구는 canonical 용어 사용.
2. **세부 타입은 괄호로만**. "Slot<T>는 slot handle (anchored)이며..." 같은 형태 권장.
3. **구체 타입명이 필요한 경우 (예: Slot vs SecureSlot 구분이 의미 있을 때)는 canonical + 구체 타입 병기**:
   - 좋음: "slot handle (anchored) 'foo: Slot<Int>' was released"
   - 피함: "Slot 'foo' was released" (canonical 결락)
4. **fix_source 토큰은 canonical 기반**:
   - 기존: `keep-handle-local-or-send-inner-value`
   - 정리 후 (선택): `keep-slot-handle-local` (canonical 따름)

---

## 후속 작업 (이번 sprint 외)

이번 sprint 산출물은 **vocabulary 표준 정의 + drift 매핑표**까지. 실제 message rewrite는 별 sprint:

1. semantic 메시지 sweep — drift 매핑표 적용
2. fix_source 토큰 일부 rename (canonical 따름) — `src/semantic/diag_codes.h`와 `docs/72`에 반영
3. user-facing tutorial/reference 문서에 canonical 4 term 정의 노출

---

## 참고

- [`src/semantic/diag_codes.h`](../src/semantic/diag_codes.h) — code/cause_ir/fix_source 매크로 registry
- [`docs/72_diagnostic_codes.md`](72_diagnostic_codes.md) — diagnostic code 카탈로그

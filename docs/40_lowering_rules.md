# RIR → MIR Lowering Rules

## 개요

이 문서는 RIR Facts JSON 출력을 MIR Instructions로 변환하는 **완전한 매핑 규칙 테이블**을 정의한다.
각 규칙은 "IF-THEN" 공식 형태로 작성되며, AI 에이전트는 이 공식에 따라Visitor 코드를 생성한다.

---

## 1. 입력 스키마: RIR Facts JSON

출처: `rir_dump_json()` in `src/compiler/rir_public_surface.h`

```jsonc
{
  "rir_version": 1,
  "scope_count": 2,
  "scopes": [
    {
      "index": 0,
      "kind": "function",         // function | method | intent | zone | relation | effect | world
      "name": "Main",
      "owner": null,
      "fact_count": 2,
      "op_count": 3,
      "facts": [
        {
          "kind": "resource",      // resource | projection | authority | capability | intent_policy
          "resource": "LOCAL_SLOT",// LOCAL_SLOT | SECURE_SLOT | DEVICE_SLOT | SUBJECT_SLOT | ...
          "name": "s",
          "slot_anchor": "s",
          "arg0": "Slot<Int>",     // 타입명 (Pergyra 표면 타입)
          "arg1": null,
          "state": "OWNED"         // UNINIT | OWNED | BORROWED_READ | RELEASED | ...
        }
      ],
      "ops": [
        {
          "kind": "CLAIM",         // CLAIM | READ | WRITE | RELEASE | MOVE | AUTHORIZE | ...
          "subject": "s",
          "slot_anchor": "s",
          "arg0": null,
          "arg1": null
        }
      ],
      "summaries": [],
      "has_state_errors": false
    }
  ]
}
```

### RIR Fact Kind → Resource 매핑

| fact.kind | fact.resource | 의미 |
|-----------|--------------|------|
| resource | LOCAL_SLOT | 일반 Slot\<T> |
| resource | SECURE_SLOT | SecureSlot\<T> (토큰 기반) |
| resource | DEVICE_SLOT | DeviceSlot\<T> (비동기) |
| resource | SUBJECT_SLOT | subject slot |
| resource | OBJECT_SLOT | object slot (read-only projection) |
| resource | REMOTE_FUTURE_HANDLE | async/await 핸들 |
| resource | QUBIT_HANDLE | 큐비트 자원 |
| projection | PROJECTION_OBJECT | object projection |
| authority | AUTHORITY_HANDLE | authority token |
| capability | CAPABILITY_TOKEN | capability token |

### RIR Op Kind 목록

| op.kind | MIR 변환 규칙 | 설명 |
|---------|--------------|------|
| CLAIM | Rule 1 | Slot Claim (할당 + 초기화) |
| READ | Rule 4 | Slot Read (값 읽기) |
| WRITE | Rule 5a | Slot Write (값 쓰기) |
| RELEASE | Rule 5b | Slot Release (자원 반납) |
| MOVE | Rule 9 | Ownership 이전 |
| BORROW_READ | Rule 10 | 불변 참조 빌림 |
| BORROW_WRITE | Rule 11 | 가변 참조 빌림 |
| PROJECT_REFRESH | Rule 12 | Projection 갱신 |
| PROJECT_PUBLISH | Rule 13 | Projection 발행 |
| AUTHORIZE | Rule 14 | 권한 검증 |
| AWAIT_REMOTE | Rule 15 | 원격 작업 대기 |
| COMMIT_INTENT | Rule 16 | Intent 커밋 |
| ABORT_INTENT | Rule 17 | Intent 중단 |
| COMPENSATE_INTENT_STEP | Rule 18 | Intent 보상 |
| ATTACH_EFFECT | Rule 19 | Effect 연결 |
| DETACH_EFFECT | Rule 20 | Effect 분리 |
| LINK_RELATION | Rule 21 | Relation 연결 |
| UNLINK_RELATION | Rule 22 | Relation 분리 |

---

## 2. 출력 스키마: MIR Instructions JSON

```jsonc
{
  "mir_version": 1,
  "routine_count": 2,
  "routines": [
    {
      "id": 0,
      "kind": "function",
      "name": "Main",
      "owner": null,
      "blocks": [
        {
          "id": 0,
          "is_entry": true,
          "instructions": [
            {
              "kind": "RESOURCE_OP",
              "name": "claim_s",
              "slot_anchor": "s",
              "arg0": "Slot<Int>",
              "arg1": null,
              "result_name": "%slot_s_1",
              "rir_op_ref": "scope[0].ops[0]",
              "type_layout": {
                "abi_type": "pgy_abi_slot_int_dbg",
                "size": 8,
                "align": 4,
                "fields": [
                  {"name": "value", "offset": 0, "size": 4},
                  {"name": "occupied", "offset": 4, "size": 1}
                ]
              },
              "runtime_fn": "pgy_claim_Int"
            }
          ]
        }
      ]
    }
  ]
}
```

---

## 3. Lowering 규칙 테이블

### Rule 1: CLAIM — Slot<T> 할당

```
입력 (RIR):
  op.kind == "CLAIM"
  fact.resource == "LOCAL_SLOT"
  fact.arg0 == "Slot<T>"  (T = Int, Long, Float, Double, Bool, String)

출력 (MIR):
  MIRInstruction {
    kind: MIR_INST_RESOURCE_OP,
    name: "claim_<subject>",
    slot_anchor: op.subject,
    arg0: fact.arg0,                       // "Slot<Int>"
    arg1: null,
    result_name: "%slot_<subject>_1",
    rir_op: <op 포인터>,
    // ABI Lookup:
    type_layout: abi_lookup(fact.arg0),    // pgy_abi_slot_int_dbg → size=8, align=4
    runtime_fn: "pgy_claim_<T>"            // "pgy_claim_Int"
  }

C 출력:
  PgySlot_Int s = pgy_claim_Int();
```

### Rule 2: CLAIM — SecureSlot<T>

```
입력 (RIR):
  op.kind == "CLAIM"
  fact.resource == "SECURE_SLOT"
  fact.arg0 == "SecureSlot<T>"

출력 (MIR):
  MIRInstruction {
    kind: MIR_INST_RESOURCE_OP,
    name: "secure_claim_<subject>",
    slot_anchor: op.subject,
    arg0: fact.arg0,
    arg1: "&token_<subject>",
    result_name: "%secure_slot_<subject>_1",
    runtime_fn: "pgy_claim_secure_<T>"
  }

C 출력:
  PgyToken_Int tok_s;
  PgySecureSlot_Int s = pgy_claim_secure_Int(&tok_s);
```

### Rule 3: CLAIM — DeviceSlot<T>

```
입력 (RIR):
  op.kind == "CLAIM"
  fact.resource == "DEVICE_SLOT"

출력 (MIR):
  MIRInstruction {
    kind: MIR_INST_RESOURCE_OP,
    name: "device_claim_<subject>",
    slot_anchor: op.subject,
    arg0: fact.arg0,
    runtime_fn: "pgy_claim_device_<T>"
  }

C 출력:
  PgyDeviceSlot_Int s = pgy_claim_device_Int();
```

### Rule 4: READ — Slot 값 읽기

```
입력 (RIR):
  op.kind == "READ"
  op.subject == "s"
  op.slot_anchor == "s"
  대응 fact: name="s", resource="LOCAL_SLOT", arg0="Slot<Int>"

출력 (MIR):
  MIRInstruction {
    kind: MIR_INST_DEF,
    name: "val_<subject>",
    slot_anchor: op.slot_anchor,
    arg0: "pgy_read_<T>",
    arg1: "&<subject>",
    result_name: "%val_<subject>_1",
    // ABI Lookup: inner type of Slot<Int> → int32_t
    type_layout: abi_inner_type(fact.arg0)  // "int32_t"
  }

C 출력:
  int32_t val_s = pgy_read_Int(&s);
```

### Rule 5a: WRITE — Slot 값 쓰기

```
입력 (RIR):
  op.kind == "WRITE"
  op.subject == "s"
  op.arg0 == "42"  (또는 결과명)

출력 (MIR):
  MIRInstruction {
    kind: MIR_INST_RESOURCE_OP,
    name: "write_<subject>",
    slot_anchor: op.subject,
    arg0: op.arg0,           // 쓸 값
    arg1: "&<subject>",
    runtime_fn: "pgy_write_<T>"
  }

C 출력:
  pgy_write_Int(&s, 42);
```

### Rule 5b: RELEASE — Slot 자원 반납

```
입력 (RIR):
  op.kind == "RELEASE"
  op.subject == "s"

출력 (MIR):
  MIRInstruction {
    kind: MIR_INST_RESOURCE_OP,
    name: "release_<subject>",
    slot_anchor: op.subject,
    arg0: "&<subject>",
    runtime_fn: "pgy_release_<T>"
  }

C 출력:
  pgy_release_Int(&s);
```

### Rule 6: MOVE — Ownership 이전

```
입력 (RIR):
  op.kind == "MOVE"
  op.subject == "src"
  op.arg0 == "dst"

출력 (MIR):
  MIRInstruction {
    kind: MIR_INST_RESOURCE_OP,
    name: "move_<src>_to_<dst>",
    slot_anchor: op.arg0,
    arg0: "&<src>",
    arg1: "&<dst>",
    runtime_fn: "pgy_slot_move_<T>"
  }

C 출력:
  dst = src;
  src.occupied = false;  /* 원본 무효화 */
```

### Rule 7: Option<T> Some 생성

```
입력 (AST → MIR 직접):
  AST_NODE_OPTION_SOME { inner_type: "Int", value: 42 }

출력 (MIR):
  MIRInstruction {
    kind: MIR_INST_DEF,
    name: "opt_some",
    arg0: "pgy_option_some_Int",
    arg1: "42",
    result_name: "%opt_1",
    type_layout: {
      abi_type: "pgy_abi_option_int",
      size: 8,
      align: 4,
      fields: [
        {"name": "tag", "offset": 0, "size": 4},
        {"name": "value", "offset": 4, "size": 4}
      ]
    }
  }

C 출력:
  PgyOption_Int opt = pgy_option_some_Int(42);
```

### Rule 8: Option<T> None 생성

```
입력: AST_NODE_OPTION_NONE { inner_type: "Int" }

출력 (MIR):
  MIRInstruction {
    kind: MIR_INST_DEF,
    name: "opt_none",
    arg0: "pgy_option_none_Int",
    arg1: null,
    result_name: "%opt_none_1",
    type_layout: abi_lookup("pgy_abi_option_int")
  }

C 출력:
  PgyOption_Int opt = pgy_option_none_Int();
```

### Rule 9: Result<T, E> Ok 생성

```
입력: AST_NODE_RESULT_OK { inner_type: "Int", value: expr }

출력 (MIR):
  MIRInstruction {
    kind: MIR_INST_DEF,
    name: "result_ok",
    arg0: "pgy_result_ok_Int",
    arg1: "<expr_result>",
    result_name: "%result_1",
    type_layout: {
      abi_type: "pgy_abi_result_int",
      size: 16,
      align: 8,
      fields: [
        {"name": "tag", "offset": 0, "size": 4},
        {"name": "ok", "offset": 8, "size": 4}
      ]
    }
  }

C 출력:
  PgyResult_Int result = pgy_result_ok_Int(42);
```

### Rule 10: Result<T, E> Try-Propagate (?)

```
입력: AST_NODE_TRY { expr: "maybe_fail()" }

출력 (MIR):
  MIRInstruction {
    kind: MIR_INST_BRANCH,
    name: "try_check",
    arg0: "%result_1.tag",
    arg1: "%result_1.ok",
    result_name: "%unwrapped_1",
    succ_true: "ok_block",     /* tag == Ok → 계속 */
    succ_false: "err_block"    /* tag == Err → early return */
  }

C 출력:
  PgyResult_Int result_1 = maybe_fail();
  if (result_1.tag != PgyResultOk) {
      return result_1;  /* early return */
  }
  int32_t unwrapped = result_1.ok;
```

### Rule 11: Channel — ZoneChannel 생성

```
입력 (RIR/AST):
  AST_NODE_CHANNEL { inner_type: "Int", capacity: 16 }
  scope.kind == "zone"  (또는 zone 내부 함수)

출력 (MIR):
  MIRInstruction {
    kind: MIR_INST_CALL,
    name: "create_ch",
    arg0: "pgy_zone_channel_create_Int",
    arg1: "%zone_arena_ptr",
    arg2: "16",
    result_name: "%ch_1",
    type_info: {
      handle_type: "pgy_abi_zone_channel_handle",
      handle_size: 4,
      owned_by: "zone"
    }
  }

C 출력:
  pgy_abi_zone_channel_handle ch = pgy_zone_channel_create_Int(zone_arena, 16);
```

### Rule 12: Channel — Send

```
입력: AST_NODE_CHANNEL_SEND { channel: "ch", value: 42 }

출력 (MIR):
  MIRInstruction {
    kind: MIR_INST_CALL,
    name: "ch_send",
    arg0: "pgy_zone_channel_send_Int",
    arg1: "%ch_1",
    arg2: "42",
    result_name: "%send_ok_1"
  }

C 출력:
  bool send_ok = pgy_zone_channel_send_Int(ch, 42);
```

### Rule 13: Channel — Recv

```
입력: AST_NODE_CHANNEL_RECV { channel: "ch", target: "x" }

출력 (MIR):
  MIRInstruction {
    kind: MIR_INST_CALL,
    name: "ch_recv",
    arg0: "pgy_zone_channel_recv_Int",
    arg1: "%ch_1",
    result_name: "%x_1"
  }

C 출력:
  int32_t x = pgy_zone_channel_recv_Int(ch);
```

### Rule 14: Channel — Close

```
입력: AST_NODE_CHANNEL_CLOSE { channel: "ch" }

출력 (MIR):
  MIRInstruction {
    kind: MIR_INST_CALL,
    name: "ch_close",
    arg0: "pgy_zone_channel_close_Int",
    arg1: "%ch_1"
  }

C 출력:
  pgy_zone_channel_close_Int(ch);
```

### Rule 15: AWAIT_REMOTE — 원격 작업 대기

```
입력 (RIR):
  op.kind == "AWAIT_REMOTE"
  op.subject == "future"

출력 (MIR):
  MIRInstruction {
    kind: MIR_INST_CALL,
    name: "await_<subject>",
    arg0: "pgy_await_export",
    arg1: "%<subject>",
    result_name: "%result_1",
    type_layout: {
      abi_type: "pgy_abi_result_int",
      size: 16
    }
  }

C 출력:
  void* raw = pgy_await_export(future);
  PgyResult_Int result = pgy_result_from_raw(raw);
```

### Rule 16: AUTHORIZE — 권한 검증

```
입력 (RIR):
  op.kind == "AUTHORIZE"
  op.subject == "token"
  op.arg0 == "action"

출력 (MIR):
  MIRInstruction {
    kind: MIR_INST_CALL,
    name: "authorize_<subject>",
    arg0: "pgy_authorize",
    arg1: "%<token>",
    arg2: "\"<action>\"",
    result_name: "%auth_ok_1"
  }

C 출력:
  bool auth_ok = pgy_authorize(&token, "action");
```

### Rule 17: COMMIT_INTENT — Intent 커밋

```
입력 (RIR):
  op.kind == "COMMIT_INTENT"

출력 (MIR):
  MIRInstruction {
    kind: MIR_INST_CALL,
    name: "commit_intent",
    arg0: "pgy_intent_commit",
    arg1: "__intent_handle"
  }

C 출력:
  pgy_intent_commit(__intent_handle);
```

---

## 4. ABI Lookup 테이블

MIR가 사용하는 타입 이름 → pgy_abi_spec.h 매핑:

| Pergyra 타입 | ABI 타입 | size | align | runtime_fn prefix |
|-------------|---------|------|-------|-------------------|
| Slot\<Int> | pgy_abi_slot_int_dbg | 8 | 4 | pgy_claim_Int |
| Slot\<Long> | pgy_abi_slot_long_dbg | 16 | 8 | pgy_claim_Long |
| Slot\<Float> | pgy_abi_slot_float_dbg | 8 | 4 | pgy_claim_Float |
| Slot\<Double> | pgy_abi_slot_double_dbg | 16 | 8 | pgy_claim_Double |
| Slot\<Bool> | pgy_abi_slot_bool_dbg | 2 | 1 | pgy_claim_Bool |
| Slot\<String> | pgy_abi_slot_string_dbg | 16 | 8 | pgy_claim_String |
| SecureSlot\<Int> | pgy_abi_secure_slot_int_dbg | 16 | 8 | pgy_claim_secure_Int |
| DeviceSlot\<Int> | pgy_abi_device_slot_int | 8 | 4 | pgy_claim_device_Int |
| Option\<Int> | pgy_abi_option_int | 8 | 4 | pgy_option_some_Int |
| Option\<Long> | pgy_abi_option_long | 16 | 8 | pgy_option_some_Long |
| Option\<String> | pgy_abi_option_string | 16 | 8 | pgy_option_some_String |
| Result\<Int> | pgy_abi_result_int | 16 | 8 | pgy_result_ok_Int |
| Result\<Bool> | pgy_abi_result_bool | 16 | 8 | pgy_result_ok_Bool |
| ZoneChannel\<Int> | pgy_abi_zone_channel_handle | 4 | 4 | pgy_zone_channel_create_Int |
| WorldChannel\<Int> | pgy_abi_world_channel_handle | 4 | 4 | pgy_world_channel_create_Int |
| Box\<Int> | pgy_abi_box_int | 8 | 8 | pgy_box_new_Int |
| Array\<Int> | pgy_abi_array_int | 24 | 8 | pgy_array_new_Int |
| Future | pgy_abi_future | 8 | 4 | pgy_spawn |
| Qubit | pgy_abi_qubit | 12 | 4 | ClaimQubit |

---

## 5. 예외 조건

| 조건 | 발생 규칙 | 처리 |
|------|---------|------|
| fact.state != OWNED 후 READ | Rule 4 | 컴파일 에러: "Read from unowned slot" |
| fact.state == RELEASED 후 READ/WRITE | Rule 4, 5a | 컴파일 에러: "Use after release" |
| ZoneChannel이 zone 밖 escape | Rule 11-14 | 컴파일 에러: "ZoneChannel cannot escape zone" |
| SECURE_SLOT에 일반 CLAIM | Rule 1 | 컴파일 에러: "SecureSlot requires secure claim" |
| Op kind == UNKNOWN | 전체 | 컴파일 에러: "Unknown RIR operation" |
| ABI lookup 실패 | 전체 | 컴파일 에러: "Unknown type in ABI spec" |

---

## 6. JSON → MIR 변환 의사 코드

```python
def rir_to_mir(rir_json, abi_spec):
    mir = {"mir_version": 1, "routine_count": 0, "routines": []}

    for scope in rir_json["scopes"]:
        routine = {
            "id": scope["index"],
            "kind": scope["kind"],
            "name": scope["name"],
            "blocks": [{"id": 0, "is_entry": True, "instructions": []}]
        }

        # Build fact map: name → fact
        facts = {f["name"]: f for f in scope["facts"]}

        for op in scope["ops"]:
            inst = lower_op(op, facts[op["subject"]], abi_spec)
            routine["blocks"][0]["instructions"].append(inst)

        mir["routines"].append(routine)
        mir["routine_count"] += 1

    return mir

def lower_op(op, fact, abi_spec):
    if op["kind"] == "CLAIM":
        if fact["resource"] == "LOCAL_SLOT":
            return rule1_claim_slot(op, fact, abi_spec)
        elif fact["resource"] == "SECURE_SLOT":
            return rule2_claim_secure(op, fact, abi_spec)
        elif fact["resource"] == "DEVICE_SLOT":
            return rule3_claim_device(op, fact, abi_spec)
    elif op["kind"] == "READ":
        return rule4_read(op, fact, abi_spec)
    elif op["kind"] == "WRITE":
        return rule5a_write(op, fact, abi_spec)
    elif op["kind"] == "RELEASE":
        return rule5b_release(op, fact, abi_spec)
    # ... 나머지 규칙

    raise ValueError(f"Unknown op: {op['kind']}")
```

---

## 7. 검증 체크리스트

각 규칙 구현 후 확인:

- [ ] MIR Instruction의 `runtime_fn`이 `pgy_abi_spec.h`의 ABI 타입과 일치하는가?
- [ ] `type_layout.size`가 static_assert 통과 값과 일치하는가?
- [ ] C 출력 코드가 기존 `pgy_runtime.h` 함수 시그니처와 일치하는가?
- [ ] Slot lifecycle 순서(CLAIM → READ/WRITE → RELEASE)가 RIR state machine과 일치하는가?
- [ ] Channel이 Zone 내부에서 생성되고 Zone 밖으로 escape하지 않는가?

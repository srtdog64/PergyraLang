# String-window 범위 감사: 호출자가 준 길이를 믿는 빌트인

작성: Claude, 2026-10-10. 읽기 전용 조사 기록이며 의미 owner가 아니다.
관찰 기준: `main @ 85fff5aa339ae136ac0797c4a6a95a5285ada70e`(다른 레인의 미커밋 변경 위),
공식 `bin/pgy.exe` SHA256 앞자리 `f6559da94876c93a`.

## 판정

**공간 메모리 안전 RED, 사용자 코드에서 재현됨.** 일곱 개 공개 빌트인
(`CharCode`, `CharAtN`, `SubstringWithLen`, `SubEqualsWithLen`, `SubContainsWithLen`,
`SubIndexOfWithLen`, `SubStartsWithLen`)은 문자열과 함께 호출자가 준 원본 길이를
받는다. 그 길이가 실제 길이와 같다는 사전 조건을 아무도 검사하지 않는다.
런타임 헤더(`src/runtime/pgy_runtime_string_window_inline.h`)도 이를 적어 두었다:
"an inflated length can read outside storage despite the index guards below."

## 재현

```text
func Main() -> Void {
    let s: String = Concat("a", "b");
    let t: String = Concat("secret-", "token");
    let mut k: Int = 0;
    let mut seen: Int = 0;
    while k < 4096 {
        let c: Int = CharCode(s, 4096, k);
        if c > 0 { seen = seen + 1; }
        k = k + 1;
    }
    Print(ToString(seen));
    Print(t);
}
```

공식 컴파일러(self-host LLVM 경로)가 경고 없이 받아들이고, 실행하면 `1639`를
출력한다. 2바이트짜리 문자열에서 0이 아닌 바이트 1639개를 읽었다는 뜻이고, 그
바이트들은 문자열 밖 힙에서 왔다. 문자열 리터럴 `"ab"`로 바꾸면 LLVM 출력이
`[3 x i8]` 상수에 `getelementptr inbounds`로 접근한다. 이는 정의되지 않은 동작이고,
최적화가 그 결과를 -1로 접는다. C 출력도 `pgy_charcode(s, 64, k)`를 그대로 넘기고,
helper는 `i < len`만 검사한 뒤 `s[i]`를 읽는다.

## 런타임만 고쳐서는 안 되는 이유

| 방식 | 판정 |
|---|---|
| 매 호출 `strnlen(s, i + 1)`로 확인 | 안전하지만 호출마다 O(i)이다. 렉서처럼 한 글자씩 훑는 루프가 O(n²)이 되어 이 빌트인이 있는 이유가 사라진다 |
| 포인터를 키로 한 마지막 길이 캐시 | 건전하지 않다. 해제 후 같은 주소를 다른 길이의 문자열이 다시 쓰면 낡은 길이를 믿는다. 소유권 정리가 문자열을 해제하므로 주소 재사용은 실제로 일어난다 |
| 길이를 앞에 붙인 문자열 표현 | 건전하고 O(1)이지만, 모든 생성 지점과 두 백엔드 리터럴, `extern "C"` 경계의 문자열을 바꿔야 한다. FFI에서 온 `char *`에는 헤더가 없다 |

## 호출 지점 조사 (정규식 조사, 증거가 아니라 탐색용)

self-host 소스 66개 파일에 일곱 빌트인의 직접 호출이 253개 있다. 각 호출의
두 번째 인자를 아래 기준으로 정규식 분류했다. 파서가 아니므로 1단계 검증기의 첫
실행이 정확한 목록을 대신한다.

| 원본 길이 인자의 출처 | 개수 |
|---|---:|
| 같은 문자열의 `StringLength`를 담은 지역 변수 | 138 |
| 같은 문자열의 `StringLength(s)`를 그 자리에서 | 24 |
| 리터럴 원본에 그 길이 이하의 상수 | 12 |
| 상수 1 이하 (`s[0]`만 읽음) | 2 |
| 매개변수나 바깥 이름 | 46 |
| 다른 식 (`i + 1`, 중첩 호출 등) | 16 |
| 다른 초기값을 가진 지역 변수 (`let n: Int = limit;` 등) | 11 |
| 리터럴이 아닌 원본에 상수 | 4 |

주의할 점이 하나 있다. `let`으로 선언한 지역 변수도 다시 대입된다
(`src/self_hosted/lib/source_size.pgy`의 `let i: Int = start;` 다음 `i = end + 1;`).
그래서 "`let n = StringLength(s)`이면 안전하다"는 소스 수준 규칙은 건전하지 않다.
`n`이나 `s`가 그 사이에 다시 대입되면 길이가 낡는다.

길이를 매개변수로 넘기는 helper(`CodegenCharCodeAt`, `JsonCharCodeAt`,
`SourceSize.ByteAfterLineSplices` 등)는 한 글자씩 훑는 루프 안에서 불린다.
helper 안에서 `StringLength`를 다시 계산하게 바꾸면 O(n²)이 된다.

## 권장 설계: MIR SSA 범위 증명 (새 문법 없음)

증명은 한 곳, 최종 MIR 승인 단계가 소유한다. C와 LLVM 백엔드는 승인된 사실만
소비한다. SSA에서는 재대입이 새 값이 되므로 "같은 값"이 구조적으로 판정된다.

- **W1 같은 값의 길이:** 길이 피연산자의 SSA 정의가 `StringLength(x)`이고, `x`가 원본
  피연산자와 같은 SSA 값이다. phi는 짝을 이루는 원본 phi의 각 입력에 대해
  재귀적으로 판정한다.
- **W2 상수:** 원본이 리터럴이고 상수가 그 리터럴 길이 이하이거나, 상수가 1 이하다.
- **W3 범위 매개변수 요약:** 함수의 (문자열 매개변수, 길이 매개변수) 짝은 모든 호출
  지점이 W1–W4로 증명된 짝을 넘길 때만 승인된다. 재귀는 고정점으로 푼다. 공개
  진입점이나 증명되지 않은 호출이 하나라도 있으면 짝은 승인되지 않는다.
- **W4 앞부분 범위:** `SubstringWithLen(json, end, ...)`처럼 앞부분만 원본으로 쓰는 경우,
  `0 <= end <= witness`를 기존 범위 사실이 증명할 때만 승인한다.
- **나머지:** 거절한다. 구조화된 진단(`string_window_extent_unproven`)과 원본 위치를
  내고, 고치는 방법으로 `StringLength(s)`를 안내한다. 검사하는 대체 경로를 몰래
  넣지 않는다.

## 닫는 순서

1. MIR 범위 사실과 그 검증기를 하나의 owner로 둔다. native 경로와 self-host
   경로가 같은 MIR을 소비하므로 판정도 한 번이다.
2. 두 백엔드의 window helper는 승인된 호출에서만 나온다. 승인되지 않은 호출을 위한
   C·LLVM 경로는 삭제하고, 그 부재를 negative gate로 고정한다.
3. W1–W4로 증명되지 않는 self-host 호출 지점을 고친다. 정규식 조사로는 약 77개이고,
   정확한 목록은 1단계 검증기의 첫 실행으로 얻는다.
4. 런타임 헤더의 주석을 "검증되지 않음"에서 "MIR 승인된 범위만 도달"로 바꾸고,
   직접 C 호출자가 남지 않았음을 확인한다.

## 반례로 고정할 것

- 위 재현 프로그램과 리터럴 판: 두 백엔드 모두 거절.
- 재대입: `let n = StringLength(s); s = "x"; CharCode(s, n, 5)`는 거절.
- phi: 한 분기에서만 `s`를 바꾼 뒤의 호출은 거절.
- 매개변수 요약: helper를 한 곳에서라도 틀린 길이로 부르면 그 helper의 모든 호출을
  승인하지 않는다.
- 양성: 위 표의 앞 네 줄 형태와 증명된 helper 사슬은 승인하고, 출력은 그대로다.

## 조율 메모

MIR 승인 계열 파일 일부(`src/compiler/mir_program_fact_validate.c` 등)는 현재
ownership-cutover 레인이 수정 중이다. 이 계획을 실행하려면 그 owner의 편집 순서를
먼저 정해야 한다. 이 감사는 코드를 바꾸지 않았다.

## 주장하지 않는 것

이 감사는 나머지 문자열 빌트인이나 언어 전체의 공간 안전을 판정하지 않는다.
재현은 Windows의 self-host LLVM 경로에서 했다. C 출력은 소스로 확인했지만 별도로
빌드해 실행하지는 않았다.

# Zone-Page 매핑 가이드 (2026-04-05)

## 한 줄 요약

> Zone은 페이지가 아니다.  
> **page/route는 projection surface**이고,  
> **zone은 execution / authority boundary**다.

---

## 1. 핵심 매핑

```
Web/App              Pergyra                역할
────────────────────────────────────────────────────────────
App 전체             world                  실행/신뢰/실패 경계
Page / Route         object/dto surface     사용자에게 보이는 투영 표면
Execution Boundary   zone                   행위/권한/효과가 검증되는 문맥
Component / Section  page fragment or zone  UI 조각 또는 행위 구역
UI Element           subject / class        행동 주체 / 도구
State                vessel                 내부 상태 수용체
API Call / Mutation  action / intent        맥락 검증된 행위 / 사용자 의지
Route Guard          requires / authorized by / intent gate
```

### 핵심 원칙

- `page`는 사용자가 보는 표면이다
- `zone`은 사용자가 행동하는 도메인 경계다
- page는 보통 `object/dto` projection을 읽고, 입력을 `intent` 또는 `action`으로 바꿔 zone을 움직인다
- API는 zone이 아니라 transport / adapter 경계다
- 따라서 `page == zone`은 기본 규칙이 아니라 특수한 단순 케이스다

### API는 어디에 놓는가

`page`와 `API`는 둘 다 표면이다. 실제 도메인 실행은 `intent`와 `zone`에서 일어난다.

```text
page / route
  -> HTTP / server action / RPC
  -> request adapter
  -> Intent(...)
  -> zone/world mutation
  -> object/dto response
```

즉:

- `page`는 projection을 읽는다
- `API`는 request/response를 적응(adapt)한다
- `intent`가 실제로 subject를 움직인다
- `zone/world`가 실행/권한/효과 경계를 가진다

쇼핑몰이라면:

```text
/cart page
  -> POST /api/intents/CheckoutPurchase
  -> HandleCheckout(request dto)
  -> CheckoutPurchase(cartZone, paymentZone, buyer)
  -> CheckoutResponse(dto)
```

따라서 `api/*.pgy`가 있다면 그 파일의 책임은 보통:

- request dto
- response dto
- identity resolution
- world/zone lookup
- `Intent(...)` 호출

까지다. zone의 business rule을 API handler 안에 직접 넣는 것은 권장하지 않는다.

---

## 2. 판단 기준: 언제 zone을 나누는가

### 2.1 zone을 나누는 신호

zone을 나눠야 하는 3가지 신호:

| 신호 | 설명 | 예시 |
|------|------|------|
| **자격이 다르다** | 서로 다른 ability가 필요 | 리뷰 작성 vs 결제 |
| **주체가 다르다** | 서로 다른 subject가 행동 | 구매자 vs 상담원 |
| **생명주기가 다르다** | 독립적으로 활성화/비활성화 | 채팅 위젯 열기/닫기 |

### 2.2 zone을 합쳐도 되는 신호

| 신호 | 설명 | 예시 |
|------|------|------|
| **같은 주체, 같은 자격, 같은 규칙** | 행위 제약이 동일 | 프로필 보기 + 수정이 완전히 같은 권한 모델 |
| **하나가 없으면 다른 하나도 의미 없음** | 생명주기가 결합 | 로그인 폼 + 비밀번호 찾기 |
| **action이 1~2개** | 나누면 오히려 과분할 | 설정 페이지의 토글 하나 |

### 2.3 page를 나누는 신호

page/route는 zone 판단과 별개다. page는 보통 사용자 경험과 내비게이션 기준으로 나눈다.

| 신호 | 설명 | 예시 |
|------|------|------|
| **내비게이션이 다르다** | URL / screen 전환이 생김 | `/cart` → `/checkout` |
| **보여주는 projection이 다르다** | 같은 zone이라도 다른 읽기 표면 | mini cart vs full cart |
| **정보 밀도가 다르다** | 같은 실행 경계를 서로 다른 UI로 표현 | 모바일 프로필 요약 vs 데스크톱 상세 |

### 2.4 판단 흐름도

```
이건 "페이지"인가 "실행 경계"인가?
  ├─ 페이지/라우트다 → object/dto projection surface로 먼저 본다
  └─ 실행 경계다
      이 구역에서 행동하는 subject가 다른가?
  ├─ YES → zone 분리
  └─ NO
      이 구역의 action에 필요한 ability가 다른가?
        ├─ YES → zone 분리
        └─ NO
            이 구역이 독립적으로 열리고 닫히는가?
              ├─ YES → zone 분리
              └─ NO → 같은 zone
```

---

## 3. 패턴별 예제

### 3.1 단순 페이지 — page 1개가 zone 1개를 거의 그대로 비추는 경우

로그인, 404, 설정처럼 단일 맥락인 페이지.

```pergyra
subject Guest
{
    let session_id: String;
}

ability Authenticatable
{
    func Authenticate(self, password: String) -> Result;
}

zone LoginZone
{
    subject slot user: Guest;

    action Login(self, id: String, pw: String)
        requires Authenticatable
        within LoginZone
    {
        // 인증 로직
    }
}

world App
{
    zone login: LoginZone;
}
```

이 경우에도 엄밀히는:

- page = 로그인 화면
- zone = 인증 실행 경계

다만 둘의 구조가 거의 같아서 1:1처럼 보여도 무방하다.

---

### 3.2 복합 페이지 — page 1개가 여러 zone을 비추는 경우

쇼핑몰 상품 페이지처럼 여러 행위 구역이 공존하는 경우.

```pergyra
// --- subject 선언 ---

subject Member
{
    let name: String;
    vessel cart: CartState;
}

subject SupportAgent
{
    let agent_id: String;
}

// --- zone 선언 ---

// 상품 정보는 보통 projection surface다.
// 별도 행위 규칙이 없다면 zone보다 object/dto 쪽이 먼저다.
object ProductSummary
{
    let title: String;
    let price: Int;
}

// 리뷰 영역: 작성 자격 필요
zone ReviewZone
{
    subject slot reviewer: Member;

    action WriteReview(self, text: String)
        requires Reviewable
        within ReviewZone
    {
        // 리뷰 작성
    }
}

// 장바구니: 구매 자격 필요
zone CartZone
{
    subject slot buyer: Member;

    action AddToCart(self, item_id: Int)
        requires Purchasable
        within CartZone
    {
        // 장바구니 추가
    }

    action Checkout(self)
        requires Purchasable
        within CartZone
        authorized by buyer
    {
        // 결제 진행
    }
}

// 채팅: 독립 생명주기 (열기/닫기)
zone SupportChatZone
{
    subject slot customer: Member;
    subject slot agent: SupportAgent;

    action SendMessage(self, msg: String)
        within SupportChatZone
    {
        // 메시지 전송
    }
}

// --- world: domain boundaries ---

world ShoppingApp
{
    zone review: ReviewZone;
    zone cart: CartZone;
    zone supportChat: SupportChatZone;
}
```

그리고 실제 page는 대략 이렇게 읽는다.

```text
ProductPage
  -> ProductSummary object/dto projection
  -> ReviewZone
  -> CartZone
  -> SupportChatZone
```

**왜 이렇게 나누는가:**

| 경계 | 역할 | 비고 |
|------|------|------|
| ProductSummary | 읽기 전용 projection | page surface |
| ReviewZone | 리뷰 작성 규칙 | 실행 경계 |
| CartZone | 장바구니/구매 규칙 | 실행 경계 |
| SupportChatZone | 고객-상담원 대화 규칙 | 실행 경계 |

즉 상품 페이지는 page 1개지만, 그 안의 도메인 경계는 여러 개다.

---

### 3.3 대시보드 — 위젯 단위 zone

```pergyra
zone StatsWidget
{
    subject slot viewer: Admin;
    // 읽기 전용 — 통계 표시
}

zone UserManagement
{
    subject slot admin: Admin;

    action BanUser(self, target_id: Int)
        requires AdminAuthority
        within UserManagement
        authorized by admin
    {
        // 사용자 차단
    }

    action PromoteUser(self, target_id: Int)
        requires AdminAuthority
        within UserManagement
        authorized by admin
    {
        // 권한 승격
    }
}

zone AuditLog
{
    subject slot auditor: Admin;
    // 읽기 전용 — 감사 로그 표시
}

world AdminDashboard
{
    zone stats: StatsWidget;
    zone users: UserManagement;
    zone audit: AuditLog;
}
```

---

### 3.4 모바일 네비게이션 — 탭은 page이고, zone일 수도 아닐 수도 있다

```pergyra
object HomeFeedView
{
    let summary: String;
}

zone SearchZone
{
    subject slot user: Member;

    action Search(self, query: String)
        within SearchZone
    {
        // 검색 실행
    }
}

zone AccountZone
{
    subject slot user: Member;

    action EditProfile(self)
        requires ProfileOwner
        within AccountZone
        authorized by user
    {
        // 프로필 수정
    }
}

zone NotificationZone
{
    subject slot user: Member;

    action MarkRead(self, notif_id: Int)
        within NotificationZone
    {
        // 읽음 처리
    }
}

world MobileApp
{
    zone search: SearchZone;
    zone account: AccountZone;
    zone notifications: NotificationZone;
}
```

탭 전환은 UI 생명주기다.  
그 자체가 곧 zone이라는 뜻은 아니다.

- Home 탭: projection 위주면 object/dto surface
- Search 탭: 실제 검색 action이 있으면 `SearchZone`
- Profile 탭: 수정/승인이 있으면 `AccountZone`
- Notification 탭: 읽음 처리 규칙이 있으면 `NotificationZone`

---

### 3.5 폼 위저드 — 단계별 zone

다단계 입력 폼 (가입, 결제 등).

```pergyra
zone Step1_BasicInfo
{
    subject slot applicant: Applicant;

    action SubmitBasicInfo(self, name: String, email: String)
        within Step1_BasicInfo
    {
        // 기본 정보 저장
    }
}

zone Step2_Verification
{
    subject slot applicant: Applicant;

    action VerifyEmail(self, code: String)
        requires Verifiable
        within Step2_Verification
    {
        // 이메일 인증
    }
}

zone Step3_Payment
{
    subject slot applicant: Applicant;

    action ProcessPayment(self, card: String)
        requires Payable
        within Step3_Payment
        authorized by applicant
    {
        // 결제 처리
    }
}

world SignupFlow
{
    zone step1: Step1_BasicInfo;
    zone step2: Step2_Verification;
    zone step3: Step3_Payment;

    // 상태 전이: step1 → step2 → step3
    state basicDone: zone step1;
    state verified: zone step2;
    state paid: zone step3;
}
```

위저드의 각 단계는 **자격이 다르다** (인증 전/후, 결제 가능/불가). 자연스럽게 zone이 분리되고, world의 `state`가 전이를 추적한다.

---

## 4. 안티패턴

### 4.1 God Zone — 페이지를 zone 하나로 뭉개는 경우

```pergyra
// BAD: 모든 행위가 한 zone에
zone ProductPageZone
{
    subject slot viewer: Member;
    subject slot reviewer: Member;
    subject slot buyer: Member;
    subject slot agent: SupportAgent;

    action WriteReview(self, ...) { ... }
    action AddToCart(self, ...) { ... }
    action Checkout(self, ...) { ... }
    action SendMessage(self, ...) { ... }
    // god zone — 자격 구분 불가, 테스트 불가
}
```

**문제:** page라는 이유만으로 같은 zone에 넣어버리면 `within` 제약이 무의미해진다. 리뷰 작성과 결제가 같은 실행 경계가 되어버린다.

### 4.2 Nano Zone — 과도한 분리

```pergyra
// BAD: action 하나당 zone 하나
zone ReviewWriteZone { action WriteReview(...) { ... } }
zone ReviewEditZone { action EditReview(...) { ... } }
zone ReviewDeleteZone { action DeleteReview(...) { ... } }
```

**문제:** 같은 주체, 같은 자격, 같은 생명주기인데 3개로 나눔. 의미 없는 분리는 복잡성만 증가시킨다.

### 4.3 판단 기준 복기

```
God Zone:   자격이 다른 action들을 한 zone에 → 분리하라
Nano Zone:  자격이 같은 action들을 여러 zone에 → 합쳐라
```

---

## 5. 프레임워크 매핑 참고

| 프레임워크 | 개념 | Pergyra 대응 |
|-----------|------|-------------|
| React | App | world |
| React | Page (react-router) | object/dto projection surface + zone 조합 |
| React | Component | zone, object, dto 중 하나의 소비 표면 |
| React | useState/useReducer | vessel |
| React | Context/Provider | zone의 subject slot |
| Next.js | Route Group | world 내 zone 그룹 |
| Next.js | Middleware | requires / authorized by |
| Flutter | MaterialApp | world |
| Flutter | Screen/Page | object/dto projection surface + zone 조합 |
| Flutter | Widget | zone consumer, object view, 또는 class |
| SwiftUI | App | world |
| SwiftUI | View + NavigationStack | object/dto projection surface + zone 조합 |
| SwiftUI | @State/@ObservedObject | vessel |

---

## 6. 설계 체크리스트

새 페이지를 설계할 때:

1. **이 페이지에서 행동하는 subject는 몇 명인가?**
   - 이 질문은 page가 아니라 zone 후보에 대해 묻는 것이다
   - 1명이고 자격도 하나 → zone 1개로 충분할 수 있다
   - 여러 명이거나 자격이 다름 → zone 분리 검토

2. **독립적으로 열리고 닫히는 구역이 있는가?**
   - UI 생명주기만 다르면 page fragment일 수 있다
   - 그 안의 규칙/권한/효과가 독립적이면 zone

3. **action의 authorized by가 다른가?**
   - 본인만 승인 vs 관리자 승인 → zone 분리

4. **읽기 전용 구역이 있는가?**
   - action 없는 순수 표시 → 보통 object/dto projection surface
   - 굳이 zone으로 올리는 것은 그 구역 자체가 독립 실행 경계일 때만

5. **나눈 zone이 3개 이상이면 → 안티패턴 점검**
   - 같은 주체 + 같은 자격인 zone이 있으면 합칠 수 있는지 확인

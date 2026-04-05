# Zone-Page 매핑 가이드 (2026-04-05)

## 한 줄 요약

> Zone은 "페이지"가 아니라 **"페이지 안의 독립된 행위 구역"**이다. 페이지는 zone의 조합이다.

---

## 1. 핵심 매핑

```
Web/App              Pergyra          역할
──────────────────────────────────────────────────
App 전체             world            실행/신뢰/실패 경계
Page / Screen        zone 조합         하나 이상의 zone으로 구성
Component / Section  zone             행위가 허용되는 독립 구역
UI Element           subject / class  행동 주체 / 도구
State                vessel           내부 상태 수용체
API Call / Mutation  action           맥락 검증된 행위
Route Guard          requires / authorized by   진입 자격
```

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
| **같은 주체, 같은 자격** | 행위 제약이 동일 | 프로필 보기 + 프로필 수정 |
| **하나가 없으면 다른 하나도 의미 없음** | 생명주기가 결합 | 로그인 폼 + 비밀번호 찾기 |
| **action이 1~2개** | 나누면 오히려 과분할 | 설정 페이지의 토글 하나 |

### 2.3 판단 흐름도

```
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

### 3.1 단순 페이지 — zone 1개 = page 1개

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

zone LoginPage
{
    subject slot user: Guest;

    action Login(self, id: String, pw: String)
        requires Authenticatable
        within LoginPage
    {
        // 인증 로직
    }
}

world App
{
    zone login: LoginPage;
}
```

**1 page = 1 zone.** 주체도 하나, 자격도 하나, 생명주기도 하나.

---

### 3.2 복합 페이지 — zone 여러 개 = page 1개

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

// 상품 정보: 읽기 전용, action 없음
zone ProductView
{
    subject slot viewer: Member;
    // action 없음 — 순수 열람
}

// 리뷰 영역: 작성 자격 필요
zone ReviewSection
{
    subject slot reviewer: Member;

    action WriteReview(self, text: String)
        requires Reviewable
        within ReviewSection
    {
        // 리뷰 작성
    }
}

// 장바구니: 구매 자격 필요
zone CartSection
{
    subject slot buyer: Member;

    action AddToCart(self, item_id: Int)
        requires Purchasable
        within CartSection
    {
        // 장바구니 추가
    }

    action Checkout(self)
        requires Purchasable
        within CartSection
        authorized by buyer
    {
        // 결제 진행
    }
}

// 채팅: 독립 생명주기 (열기/닫기)
zone ChatWidget
{
    subject slot customer: Member;
    subject slot agent: SupportAgent;

    action SendMessage(self, msg: String)
        within ChatWidget
    {
        // 메시지 전송
    }
}

// --- world: page = zone 조합 ---

world ShoppingApp
{
    // "상품 페이지" = 4개 zone
    zone productView: ProductView;
    zone reviewSection: ReviewSection;
    zone cartSection: CartSection;
    zone chatWidget: ChatWidget;
}
```

**왜 나눴는가:**

| zone | 주체 | 자격 | 생명주기 |
|------|------|------|----------|
| ProductView | Member (열람) | 없음 | 항상 활성 |
| ReviewSection | Member (작성) | Reviewable | 항상 활성 |
| CartSection | Member (구매) | Purchasable | 항상 활성 |
| ChatWidget | Member + Agent | 없음 | 독립 (열기/닫기) |

주체가 다르거나(ChatWidget), 자격이 다르거나(Review vs Cart), 생명주기가 다르면(ChatWidget) 분리.

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

### 3.4 모바일 네비게이션 — 탭 = zone

```pergyra
zone HomeTab
{
    subject slot user: Member;
    // 피드 표시
}

zone SearchTab
{
    subject slot user: Member;

    action Search(self, query: String)
        within SearchTab
    {
        // 검색 실행
    }
}

zone ProfileTab
{
    subject slot user: Member;

    action EditProfile(self)
        requires ProfileOwner
        within ProfileTab
        authorized by user
    {
        // 프로필 수정
    }
}

zone NotificationTab
{
    subject slot user: Member;

    action MarkRead(self, notif_id: Int)
        within NotificationTab
    {
        // 읽음 처리
    }
}

world MobileApp
{
    zone home: HomeTab;
    zone search: SearchTab;
    zone profile: ProfileTab;
    zone notifications: NotificationTab;
}
```

모바일 탭은 **생명주기가 독립적**(탭 전환)이므로 자연스럽게 zone 분리.

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

### 4.1 God Zone — 모든 것을 하나에

```pergyra
// BAD: 모든 행위가 한 zone에
zone ProductPage
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

**문제:** 모든 action이 같은 zone에 있으므로 `within` 제약이 무의미해진다. 리뷰 작성과 결제가 같은 맥락에서 허용되는 셈.

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
| React | Page (react-router) | zone 조합 |
| React | Component | zone (행위 있으면) 또는 object (읽기 전용이면) |
| React | useState/useReducer | vessel |
| React | Context/Provider | zone의 subject slot |
| Next.js | Route Group | world 내 zone 그룹 |
| Next.js | Middleware | requires / authorized by |
| Flutter | MaterialApp | world |
| Flutter | Screen/Page | zone 조합 |
| Flutter | Widget | zone 또는 class |
| SwiftUI | App | world |
| SwiftUI | View + NavigationStack | zone 조합 |
| SwiftUI | @State/@ObservedObject | vessel |

---

## 6. 설계 체크리스트

새 페이지를 설계할 때:

1. **이 페이지에서 행동하는 subject는 몇 명인가?**
   - 1명이고 자격도 하나 → zone 1개
   - 여러 명이거나 자격이 다름 → zone 분리 검토

2. **독립적으로 열리고 닫히는 구역이 있는가?**
   - 모달, 드로어, 채팅 위젯 → 별도 zone

3. **action의 authorized by가 다른가?**
   - 본인만 승인 vs 관리자 승인 → zone 분리

4. **읽기 전용 구역이 있는가?**
   - action 없는 순수 표시 → zone (action 없음) 또는 object로 투영

5. **나눈 zone이 3개 이상이면 → 안티패턴 점검**
   - 같은 주체 + 같은 자격인 zone이 있으면 합칠 수 있는지 확인

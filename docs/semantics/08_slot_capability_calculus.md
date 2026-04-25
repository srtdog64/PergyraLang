# 08. Slot Capability Calculus

Last updated: 2026-04-25
Status: `IN PROGRESS`

이 문서는 Pergyra의 핵심인 Slot System과 Token 권한 검증을 엄밀한 수학적 조작적 의미론(Operational Semantics)으로 정의한 **Capability Calculus** 명세입니다.

Rust가 컴파일 타임에 '지역 변수의 생명주기'를 증명한다면, Pergyra는 런타임 상태 전이(State Transition) 과정에서 '권한(Capability)의 무결성'을 증명합니다.

## 1. Semantic Domains (의미론적 도메인)

Pergyra 메모리 안전성을 증명하기 위한 상태 공간은 다음과 같이 정의됩니다.

*   $\Sigma$ (Slot Heap): 슬롯 메모리의 전역 상태. 슬롯 식별자 $l$ 을 슬롯 레코드 $\langle v, \tau, gen, ttl, \pi \rangle$ 로 매핑.
    *   $v$: 실제 저장된 값 (Value)
    *   $\tau$: 타입 태그 (TypeTag)
    *   $gen$: 생성 세대 카운터 (Generation) - ABA 문제 방지
    *   $ttl$: 만료 시간 (Time-to-Live)
    *   $\pi$: Pin 상태 (Pinned or Unpinned)
*   $\Delta$ (Capability Environment): 현재 스레드/태스크가 보유한 토큰(권한)들의 집합. $k \in \Delta$ (where $k$ is a TokenCapability).
*   $\Gamma$ (Local Environment): 변수 바인딩. $x \mapsto \langle l, gen \rangle$ (변수 $x$가 슬롯 $l$의 $gen$ 세대를 가리킴).

전역 상태 공간은 $S = \langle \Gamma, \Sigma, \Delta \rangle$ 입니다.

## 2. Capability Functions

토큰 $k$ 가 슬롯 $l$ 의 세대 $gen$ 에 대해 유효한 접근 권한(Mode $m \in \{R, W, P\}$)을 가지는지 확인하는 함수:

$$
\text{Verify}(k, l, gen, m) =
\begin{cases}
    \text{true} & \text{if } k \text{ is a valid cryptographic capability for } (l, gen) \text{ with mode } m \\
    \text{false} & \text{otherwise}
\end{cases}
$$

## 3. Inference Rules (상태 전이 규칙)

### 3.1 Slot Claim (할당)
새로운 슬롯을 할당하면 새로운 식별자 $l$ 과 세대 $gen=1$ 이 생성되며, 최고 권한의 토큰 $k_{master}$ 가 $\Delta$ 에 추가됩니다.

$$
\frac{l \notin dom(\Sigma) \quad \Sigma' = \Sigma[l \mapsto \langle \text{null}, \tau, 1, \infty, \text{Unpinned} \rangle] \quad \Delta' = \Delta \cup \{k_{master}(l, 1)\}}{\langle \Gamma, \Sigma, \Delta \rangle \xrightarrow{\text{Claim}(\tau)} \langle \Gamma[x \mapsto \langle l, 1 \rangle], \Sigma', \Delta' \rangle}
$$

### 3.2 Slot Read (읽기)
읽기 작업은 $\Gamma$ 에 저장된 슬롯 핸들이 $\Sigma$ 의 실제 세대와 일치하고, $\Delta$ 에 유효한 읽기 토큰($R$)이 있을 때만 진행됩니다.

$$
\frac{\Gamma(x) = \langle l, gen \rangle \quad \Sigma(l) = \langle v, \tau, gen', ttl, \pi \rangle \quad gen = gen' \quad \text{Verify}(\Delta, l, gen, R)}{\langle \Gamma, \Sigma, \Delta \rangle \xrightarrow{\text{Read}(x)} v}
$$
> *증명 의무(Lemma: ABA Safe):* 만약 슬롯이 해제되고 재할당되어 $gen' > gen$ 이 되었다면, $gen = gen'$ 조건이 거짓이 되므로 런타임 읽기가 차단됨.

### 3.3 Slot Pin (고속 캐싱)
Pinning은 권한을 1회 검증한 후, $\Sigma$ 의 Pin 상태를 활성화합니다. Pinned 슬롯은 GC나 TTL 만료에서 제외됩니다.

$$
\frac{\Gamma(x) = \langle l, gen \rangle \quad \Sigma(l) = \langle v, \tau, gen, ttl, \text{Unpinned} \rangle \quad \text{Verify}(\Delta, l, gen, R \cup W)}{\langle \Gamma, \Sigma, \Delta \rangle \xrightarrow{\text{Pin}(x)} \langle \Gamma, \Sigma[l \mapsto \langle v, \tau, gen, ttl, \text{Pinned} \rangle], \Delta \rangle}
$$

### 3.4 Slot Release (해제)
슬롯을 명시적으로 해제하면 $gen$ 이 증가하거나 $\Sigma$ 에서 제거됩니다. 단, **Pinned 상태인 슬롯은 해제할 수 없습니다.**

$$
\frac{\Gamma(x) = \langle l, gen \rangle \quad \Sigma(l) = \langle v, \tau, gen, ttl, \text{Unpinned} \rangle \quad \text{Verify}(\Delta, l, gen, W)}{\langle \Gamma, \Sigma, \Delta \rangle \xrightarrow{\text{Release}(x)} \langle \Gamma, \Sigma[l \mapsto \bot], \Delta \rangle}
$$

## 4. Core Theorems

### Theorem 1: Token Unforgeability (권한 위조 불가능성)
어떤 연산의 연쇄 $\rightarrow^*$ 후에도, $\text{Claim}$ 이나 $\text{HandOff}$ (토큰 전달) 규칙을 통하지 않고 생성된 임의의 토큰 $k_{fake}$ 에 대해 $\text{Verify}(k_{fake}, l, gen, m) = \text{true}$ 가 되는 경우는 존재하지 않는다.

### Theorem 2: Pin Non-Eviction (고정 슬롯 불변성)
슬롯 $l$ 의 상태가 $\pi = \text{Pinned}$ 라면, $\text{Unpin}(l)$ 이 호출되기 전까지 어떠한 $\text{Release}(l)$ 나 백그라운드 $\text{Evict}(l)$ 규칙도 적용될 수 없다. (이는 상단 3.4 규칙의 전제 조건인 $\pi = \text{Unpinned}$ 에 의해 자명하게 증명됨).

## 5. Next Steps
향후 이 수식들은 TLA+ (Temporal Logic of Actions) 스펙으로 변환되어 모델 체킹(Model Checking)을 통해 동시성 스레드 접근(Concurrent Access) 하에서의 교착 상태(Deadlock) 및 토큰 누수(Token Leak)가 없음을 기계적으로 증명할 예정입니다.

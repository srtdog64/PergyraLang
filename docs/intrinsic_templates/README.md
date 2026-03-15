# Pergyra Intrinsic Templates

AI가 길고 반복적인 보일러플레이트를 직접 생성하지 않고도 Pergyra를 안정적으로 사용할 수 있게 만드는 내장 템플릿 축이다.

이 디렉터리는 다음 목적을 가진다.

- AI 친화 intrinsic template의 범위와 원칙 정의
- 컴파일러 내부 확장 지점 정리
- 초기 내장 템플릿 API 후보 관리

## 문서 목록

- `01_design_goals.md` -- 개념, 목표, 비목표, 용어
- `02_expansion_pipeline.md` -- 파싱 이후 확장 파이프라인과 구현 위치
- `03_initial_catalog.md` -- 초기 intrinsic template API 후보
- `04_pattern_vault_strategy.md` -- generic-to-domain injection 패턴 저장소와 intrinsic 연결 전략

## 핵심 방향

- 에디터 스니펫이나 외부 플러그인에 의존하지 않는다.
- AI는 짧고 정규화된 API 호출만 출력한다.
- 컴파일러가 해당 호출을 intrinsic template로 해석하고 확장한다.
- 확장 이후 결과물은 일반 Pergyra 코드와 동일한 타입 검사와 코드 생성을 거친다.

즉, intrinsic template는 "AI 전용 편의 문법"이 아니라 "언어가 직접 제공하는 축약형 표면 API"로 본다.

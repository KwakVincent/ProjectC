# Challenge: 반사 계층 (Reflex Layer) 초고속 반응 아키텍처

## 1. 개요 (Overview)
- **목표**: STT(Speech To Text, 음성 인식) 완료 시점부터 로봇의 물리적 반응 개시까지의 지연 시간을 0~5ms 이내로 단축하는 초고속 생체모사 신경망 아키텍처.
- **배경**: On-Device SLM(Small Language Model, 소형 언어 모델)의 추론 시간이 20~50ms까지 단축되더라도, 사용자가 "말이 끝나는 즉시 반응한다"는 피지컬적 충격을 주기 위해 생체 척수 반사(Spinal Reflex) 메커니즘을 소프트웨어적으로 구현.
- **도전 시점**: Action Pool 및 On-Device LLM 기반 액션 선택 시스템의 기본 기능이 안정화된 후 극한의 반응속도 최적화 단계에서 적용.

---

## 2. 작동 원리 (Mechanics)
1. **생체 척수 반사 분리 원리**:
   - 인간이 뜨거운 것에 닿았을 때 대뇌가 생각하기 전에 척수가 먼저 근육을 수축하듯, '반사(Reflex)'와 '심사숙고(Deliberation)' 계층을 분리.
2. **초고속 토큰/키워드 감지 (O(N) 매처, < 0.1ms)**:
   - 전체 문맥 파싱 없이, 입력된 텍스트의 앞부분 1~2개 어절에서 지배적인 방향(Dominant Direction: 전진, 후진, 좌, 우) 및 긴급 키워드(정지, 회피)를 즉각 도출.
3. **선점 이동 및 선딜레이 흡수**:
   - 로봇이 정지 상태에서 최고 속도에 도달하기까지의 물리적 가속도 및 턴 애니메이션 선딜레이(Acceleration Transition Phase, 약 50~100ms) 동안 반사 계층이 먼저 발을 딛도록 명령.
4. **LLM 추론 결과와의 궤적 융합 (Blending)**:
   - 20~40ms 후 On-Device LLM이 고도화된 액션(예: 단순 걷기가 아닌 돌격 대시, 스플라인 우회 등)을 확정하면, 이미 움직이고 있던 캐릭터의 액션 상태를 부드럽게 승격/보정.

---

## 3. ECS (Entity Component System) 연계 설계
- **컴포넌트 (`FMovementIntentComponent`)**:
  - `EIntentSource mSource`: `None` -> `Reflex` (0.1ms) -> `LLM` (30ms 덮어쓰기)
  - `FVector mDirectionVector`: 목표 방향
  - `float mSpeedMultiplier`: 속도 배율
  - `FName mTargetActionId`: 최종 결정된 액션 식별자
- **시스템 (`UVKReflexSystem`)**:
  - STT 수신 즉시 `mSource = Reflex`로 1차 방향 및 기본 이동 주입.
- **시스템 (`UVKLLMActionSelectorSystem`)**:
  - LLM 추론 완료 즉시 `mSource = LLM`으로 전환하고 최종 고도화 액션 덮어쓰기.

---

## 4. 기대 효과 (Expected Outcome)
- **체감 반응 속도**: 1~5ms (완전한 무지연 체감)
- **지능과 피지컬의 양립**: 즉각적인 반응성과 LLM의 풍부한 상황 맥락 이해를 동시에 달성.

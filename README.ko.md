[English](README.md)

# L4D2 Outbreak Experience 로컬 인원수 수정 모드

직접 호스트한 레프트 4 데드 2 로컬 협동 캠페인의 생존자 정원을 4~8명으로 조절하는 Neko 플러그인입니다. 빈 생존자 자리는 봇이 채웁니다.

**[0.1.4-test 다운로드](https://github.com/Hu-Myo/L4D2-Outbreak-Experience-Local-Coop-Slots-Neko/releases/download/v0.1.4-test/L4D2-Outbreak-Experience-Local-Coop-Slots-Neko_0.1.4-test.zip)**

Windows용 플러그인은 한국 시간 2026년 11월 25일에 설치된 Steam 빌드 `26770911`과 이전에 확인한 게임 빌드를 지원합니다. 두 빌드 모두 게임 버전은 `2.2.4.3`으로 표시됩니다.

> **이용 제한·계정 정지 안내:** 이 DLL은 게임 실행 중 동작을 변경합니다. 사용으로 인해 게임·서버 이용 제한이나 계정 정지가 발생하지 않는다고 보장할 수 없습니다. 관련 법률이 허용하는 범위에서 제작자는 이러한 제재에 대해 책임지지 않습니다. 이러한 이용 제한·계정 정지 위험을 이해한 뒤 본인의 판단으로 사용해 주세요.

[원본과 수정판의 책임 범위](RESPONSIBILITY.ko.md)

## 설치 및 사용

1. 제작자 안내에 따라 [NekoShaders & L4N](https://steamcommunity.com/sharedfiles/filedetails/?id=3558049615)을 설치합니다. 지원되는 Windows 게임 빌드에서 Left4Neko 2.51.0을 사용합니다.
2. 게임을 종료합니다. 릴리스 ZIP을 풀고 `NekoLocalCoopSlots.dll`을 `<Left 4 Dead 2>/bin/neko/plugins/NekoLocalCoopSlots.dll`에 넣습니다. 기존 DLL이 있으면 교체 전에 백업합니다.
3. 로컬 협동 캠페인을 직접 호스트합니다. 맵이 로드되면 콘솔에 `l4n_menu`를 입력하고 **L4N 플러그인**에서 인원수 메뉴를 열어 4~8명 중 하나를 선택합니다. 게임 언어가 한국어일 때는 **로컬 협동 인원수**, 그 외에는 **Local Co-op Player Count**로 표시됩니다.

### 선택 사항 · 추가 생존자 HUD

[추가 생존자 HUD](https://github.com/Hu-Myo/L4D2-Outbreak-Experience-HUD-Neko/releases/tag/v0.1.27-test)도 함께 설치하면 인원수를 늘렸을 때 추가된 팀원의 초상화와 체력 바를 HUD에 표시할 수 있습니다. 현재 설치된 게임 빌드를 지원하는 HUD 버전이 필요합니다.

## 캠페인에서 달라지는 점

- 같은 캠페인의 다음 챕터에도 선택한 정원이 적용됩니다. 일반 로비는 4명으로 시작합니다. 8인 로비 요청을 받으면 해당 로컬 서버가 지원하는 경우 8명으로 맞춥니다.
- 정원을 낮추면 남는 봇만 내보냅니다. 플레이어가 사용 중인 자리와 잠수 중 예약된 봇 자리는 보호합니다.
- 레포데 2 생존자 캠페인에서는 빌·조이·프란시스·루이스가, 레포데 1 생존자 캠페인에서는 닉·로셸·코치·엘리스가 추가 자리를 채웁니다.
- 0.1.2 테스트 빌드는 레포데 1 캠페인 크레딧에서 추가된 레포데 2 생존자의 이름을 바로잡습니다. 기존 통계는 각자의 이름과 연결됩니다.

메뉴는 로컬 호스트에게 표시됩니다. Steam 초대와 일반 로비 정원은 Steam의 기존 규칙을 따릅니다. 게임 모듈이 호환성 검사와 일치하지 않으면 기본 동작이 적용됩니다. 로그는 `left4dead2/neko/local_coop_slots.log`에 기록됩니다.

## 소스 빌드

[SDK 준비 안내](sdk/README.md)에 따라 Neko SDK 헤더를 `sdk/l4n_plugin.h`에 넣습니다. Visual Studio 2022의 x86 Native Tools Command Prompt에서 `build.cmd`를 실행하면 `build/NekoLocalCoopSlots.dll`이 생성됩니다.

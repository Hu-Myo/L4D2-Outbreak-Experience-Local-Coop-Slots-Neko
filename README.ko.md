[English](README.md)

# L4D2 Outbreak Experience 로컬 인원수 수정 모드

직접 호스트한 레프트 4 데드 2 로컬 협동 캠페인의 생존자 정원을 4~8명으로 조절하는 Neko 플러그인입니다. 빈 생존자 자리는 봇이 채웁니다.

**[0.1.2-test 다운로드](https://github.com/Hu-Myo/L4D2-Outbreak-Experience-Local-Coop-Slots-Neko/releases/download/v0.1.2-test/L4D2-Outbreak-Experience-Local-Coop-Slots-Neko_0.1.2-test.zip)**

## 설치 및 사용

1. 제작자 안내에 따라 [NekoShaders & L4N](https://steamcommunity.com/sharedfiles/filedetails/?id=3558049615)을 설치합니다. 이 빌드는 Windows판 L4D2 2.2.4.3과 Left4Neko 2.51.0에서 테스트했습니다.
2. 게임을 종료합니다. 릴리스 ZIP을 풀고 `NekoLocalCoopSlots.dll`을 `<Left 4 Dead 2>/bin/neko/plugins/NekoLocalCoopSlots.dll`에 넣습니다. 기존 DLL이 있으면 교체 전에 백업합니다.
3. 로컬 협동 캠페인을 직접 호스트합니다. 맵이 로드되면 콘솔에 `l4n_menu`를 입력하고 **L4N 플러그인 → 로컬 협동 인원수**에서 4~8명 중 하나를 선택합니다.

## 캠페인에서 달라지는 점

- 같은 캠페인의 다음 챕터에도 선택한 정원이 적용됩니다. 일반 로비는 4명으로 시작합니다. 8인 로비 요청을 받으면 해당 로컬 서버가 지원하는 경우 8명으로 맞춥니다.
- 정원을 낮추면 남는 봇만 내보냅니다. 플레이어가 사용 중인 자리와 잠수 중 예약된 봇 자리는 보호합니다.
- 레포데 2 생존자 캠페인에서는 빌·조이·프란시스·루이스가, 레포데 1 생존자 캠페인에서는 닉·로셸·코치·엘리스가 추가 자리를 채웁니다.
- 0.1.2 테스트 빌드는 레포데 1 캠페인 크레딧에서 추가된 레포데 2 생존자의 이름을 바로잡습니다. 기존 통계는 각자의 이름과 연결됩니다.

메뉴는 로컬 호스트에게 표시됩니다. Steam 초대와 일반 로비 정원은 Steam의 기존 규칙을 따릅니다. 게임 모듈이 호환성 검사와 일치하지 않으면 기본 동작이 적용됩니다. 로그는 `left4dead2/neko/local_coop_slots.log`에 기록됩니다.

## 소스 빌드

[SDK 준비 안내](sdk/README.md)에 따라 Neko SDK 헤더를 `sdk/l4n_plugin.h`에 넣습니다. Visual Studio 2022의 x86 Native Tools Command Prompt에서 `build.cmd`를 실행하면 `build/NekoLocalCoopSlots.dll`이 생성됩니다.

[한국어 안내](README.ko.md)

# L4D2 Outbreak Experience Local Co-op Slots (Built with Neko)

Change the survivor limit from four to eight in a Left 4 Dead 2 co-op campaign you host. The plugin adds a player-count menu to Neko and fills empty survivor slots with bots.

**[Download 0.1.3-test](https://github.com/Hu-Myo/L4D2-Outbreak-Experience-Local-Coop-Slots-Neko/releases/download/v0.1.3-test/L4D2-Outbreak-Experience-Local-Coop-Slots-Neko_0.1.3-test.zip)**

## Install and use

1. Install [NekoShaders & L4N](https://steamcommunity.com/sharedfiles/filedetails/?id=3558049615) using the author's instructions. This build was tested with Windows L4D2 2.2.4.3 and Left4Neko 2.51.0.
2. Close the game. Extract the release ZIP and place `NekoLocalCoopSlots.dll` at `<Left 4 Dead 2>/bin/neko/plugins/NekoLocalCoopSlots.dll`. Back up any existing copy before replacing it.
3. Host a local co-op campaign. After the map loads, enter `l4n_menu` in the game console. Open **L4N Plugins**, select **Local Co-op Player Count**, and choose four to eight survivors. If the game language is Korean, this menu is labeled **로컬 협동 인원수**. All other game languages use the English labels.

### Optional teammate HUD

Install the [Extra Survivor HUD](https://github.com/Hu-Myo/L4D2-Outbreak-Experience-HUD-Neko/releases/tag/v0.1.27-test) alongside this plugin to show portraits and health bars for the additional teammates.

## What changes in a campaign

- The host's selection carries over to later chapters in the same campaign. Regular lobbies start with four slots, and an eight-seat lobby request sets the local host limit to eight when supported.
- Reducing the limit removes only spare bots. The plugin keeps slots occupied by players and bots reserved for idle players.
- Extra slots use the opposite survivor cast. L4D2 campaigns get Bill, Zoey, Francis, and Louis. L4D1 campaigns get Nick, Rochelle, Coach, and Ellis.
- The 0.1.2 test build corrects the credits labels for added L4D2 survivors on L4D1 campaigns. Their existing statistics stay attached to the correct names.

The menu is available to the local host. Steam invites and ordinary lobby capacity follow Steam's existing rules. Compatibility checks preserve stock behavior when the installed game modules do not match. The runtime log is `left4dead2/neko/local_coop_slots.log`.

## Build from source

Place the Neko SDK header at `sdk/l4n_plugin.h` following [the SDK instructions](sdk/README.md). From an x86 Native Tools Command Prompt for Visual Studio 2022, run `build.cmd`. The output is `build/NekoLocalCoopSlots.dll`.

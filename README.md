[한국어 안내](README.ko.md)

# L4D2 Outbreak Experience Local Co-op Slots (Built with Neko)

Change the survivor limit from four to eight in a Left 4 Dead 2 co-op campaign you host. The plugin adds a player-count menu to Neko and fills empty survivor slots with bots.

**[Download 0.1.6-test](https://github.com/Hu-Myo/L4D2-Outbreak-Experience-Local-Coop-Slots-Neko/releases/download/v0.1.6-test/L4D2-Outbreak-Experience-Local-Coop-Slots-Neko_0.1.6-test.zip)**

The Windows plugin supports Steam build `26770911` and the previous verified build. It can also load after an update changes the file hashes if the game functions, object layouts, and hook sites it uses still match build `26770911`.

> **Account and server restriction notice:** This DLL modifies the game at runtime. The author cannot guarantee that using it will not result in game/server restrictions or account bans. To the extent permitted by law, the author accepts no responsibility for such penalties. Use it only if you accept these risks of restrictions or bans.

[Responsibility for original and modified versions](RESPONSIBILITY.md)

## Install and use

1. Install [NekoShaders & L4N](https://steamcommunity.com/sharedfiles/filedetails/?id=3558049615) using the author's instructions. Use Left4Neko 2.51.0 with a supported Windows game build.
2. Close the game. Extract the release ZIP and place `NekoLocalCoopSlots.dll` at `<Left 4 Dead 2>/bin/neko/plugins/NekoLocalCoopSlots.dll`. Back up any existing copy before replacing it.
3. Host a local co-op campaign. After the map loads, enter `l4n_menu` in the game console. Open **L4N Plugins**, select **Local Co-op Player Count**, and choose four to eight survivors. If the game language is Korean, this menu is labeled **로컬 협동 인원수**. All other game languages use the English labels.

### Optional teammate HUD

Install the [Extra Survivor HUD](https://github.com/Hu-Myo/L4D2-Outbreak-Experience-HUD-Neko/releases/latest) alongside this plugin to show cards for additional teammates. See its release notes for game compatibility.

## What changes in a campaign

- The host's selection carries over to later chapters in the same campaign. Regular lobbies start with four slots, and an eight-seat lobby request sets the local host limit to eight when supported.
- Reducing the limit removes only spare bots. The plugin keeps slots occupied by players and bots reserved for idle players.
- Extra slots use the opposite survivor cast. L4D2 campaigns get Bill, Zoey, Francis, and Louis. L4D1 campaigns get Nick, Rochelle, Coach, and Ellis.
- The Passing keeps its scripted survivor appearances when the host adds survivors.
- The credits show the corrected names for added L4D2 survivors on L4D1 campaigns. Their existing statistics stay attached to the correct names.

The menu is available to the local host. Steam invites and ordinary lobby capacity follow Steam's existing rules. If a required compatibility check fails, the game keeps its stock local slots. The credits name fix has its own check. The runtime log is `left4dead2/neko/local_coop_slots.log`.

## Build from source

Place the Neko SDK header at `sdk/l4n_plugin.h` following [the SDK instructions](sdk/README.md). From an x86 Native Tools Command Prompt for Visual Studio 2022, run `build.cmd`. The output is `build/NekoLocalCoopSlots.dll`.

From the same prompt, run `tests\verify.cmd "<Left 4 Dead 2>"` with the reference build to verify compatibility checks against isolated game DLL copies without starting the game.

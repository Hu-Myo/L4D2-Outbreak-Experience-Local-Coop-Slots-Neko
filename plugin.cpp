#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>

#pragma warning(push)
#pragma warning(disable : 4100)
#include "l4n_plugin.h"
#pragma warning(pop)

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>

#if !defined(_M_IX86)
#error Use the x86 toolchain for the Source game server modules.
#endif

namespace {

constexpr int kStockLimit = 4;
constexpr int kLargestLimit = 8;
constexpr int kSurvivorTeam = 2;

// RVAs and layouts verified against the three module hashes checked at startup.
constexpr std::size_t kGameRules = 0x7F7F6C;
constexpr std::size_t kDirector = 0x83B1B8;
constexpr std::size_t kGlobals = 0x7F7690;
constexpr std::size_t kEngineServer = 0x7F7698;
constexpr std::size_t kGameServer = 0x645558;
constexpr std::size_t kServerState = 4;
constexpr std::size_t kMaxClients = 0x104;
constexpr std::size_t kDedicated = 0x128;
constexpr std::size_t kServerSlots = 0x180;
constexpr std::size_t kMatchFramework = 0x6E0B14;
constexpr std::size_t kConVarParent = 0x1C;
constexpr std::size_t kConVarString = 0x24;
constexpr std::size_t kConVarInt = 0x30;
constexpr std::size_t kConVarHasMax = 0x3C;
constexpr std::size_t kConVarMax = 0x40;

using ActivateFn = void(__thiscall*)(void*, void*, int, int);
using LevelInitFn = bool(__thiscall*)(void*, const char*, const char*, const char*, const char*, bool, bool);
using FrameFn = void(__thiscall*)(void*, bool);
using ShutdownFn = void(__thiscall*)(void*);
using RulesLimitFn = int(__thiscall*)(void*);
using SetIntFn = void(__thiscall*)(void*, int);
using FactoryFn = void*(__cdecl*)(const char*, int*);
using FindVarFn = void*(__thiscall*)(void*, const char*);
using PlayerByIndexFn = void*(__cdecl*)(int);
using AddSurvivorFn = void(__thiscall*)(void*, int);

std::uint8_t* g_engine = nullptr;
std::uint8_t* g_server = nullptr;
FactoryFn g_cvarFactory = nullptr;
void* g_playerInfoManager = nullptr;
std::atomic<void*> g_activeRules{nullptr};
std::atomic<int> g_effective{4};
std::atomic<unsigned> g_menuContext{0};  // Upper bits identify the hosted map, low byte holds its maximum.
std::atomic<unsigned> g_request{0};
std::atomic<bool> g_reduceBlocked{false};
std::atomic<int> g_lastMenuVisibility{-1};
std::atomic<bool> g_starting{false};
std::atomic<unsigned> g_creditsCharacters{0};
std::uint8_t* g_creditsClient = nullptr;
bool g_creditsAttempted = false;

struct MapState {
    void* rules = nullptr;
    int originalServerSlots = 0;
    bool ownsServerSlots = false;
    int highest = 0;
    int survivorSet = 0;
    int reducingTo = 0;
    ULONGLONG reduceDeadline = 0;
    ULONGLONG nextCheck = 0;
    unsigned checksLeft = 0;
};

MapState g_map;
void* g_survivorLimit = nullptr;
int g_sessionLimit = kStockLimit;
bool g_playableLevel = false;
bool g_waitForActivation = false;
unsigned g_mapEpoch = 0;

template <typename T>
T Read(const void* base, std::size_t offset) {
    T result;
    std::memcpy(&result, static_cast<const std::uint8_t*>(base) + offset, sizeof(T));
    return result;
}

template <typename T>
void Write(void* base, std::size_t offset, T value) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, sizeof(T));
}

template <typename T>
T Method(const void* object, std::size_t slot) {
    return reinterpret_cast<T>((*static_cast<void* const* const*>(object))[slot]);
}

void Log(const char* format, ...) {
    wchar_t path[MAX_PATH];
    const auto length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (!length || length >= MAX_PATH) return;
    auto* slash = std::wcsrchr(path, L'\\');
    if (!slash || wcscpy_s(slash + 1, MAX_PATH - (slash + 1 - path),
                          L"left4dead2\\neko\\local_coop_slots.log") != 0) return;

    FILE* file = nullptr;
    if (_wfopen_s(&file, path, L"a") != 0) return;
    SYSTEMTIME now;
    GetLocalTime(&now);
    std::fprintf(file, "%04u-%02u-%02u %02u:%02u:%02u [0.1.2-test] ",
                 now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    va_list args;
    va_start(args, format);
    std::vfprintf(file, format, args);
    va_end(args);
    std::fputc('\n', file);
    std::fclose(file);
}

bool MatchesHash(HMODULE module, const char* expected) {
    wchar_t path[MAX_PATH];
    const auto length = GetModuleFileNameW(module, path, MAX_PATH);
    if (!length || length >= MAX_PATH) return false;

    FILE* file = nullptr;
    if (_wfopen_s(&file, path, L"rb") != 0) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::array<UCHAR, 32> digest{};
    bool ok = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0 &&
              BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) == 0;
    std::array<UCHAR, 64 * 1024> buffer;
    while (ok) {
        const auto size = std::fread(buffer.data(), 1, buffer.size(), file);
        if (!size) break;
        ok = BCryptHashData(hash, buffer.data(), static_cast<ULONG>(size), 0) == 0;
    }
    ok = ok && !std::ferror(file) && BCryptFinishHash(hash, digest.data(), digest.size(), 0) == 0;
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    std::fclose(file);

    constexpr char hex[] = "0123456789abcdef";
    for (std::size_t i = 0; ok && i < digest.size(); ++i) {
        ok = expected[i * 2] == hex[digest[i] >> 4] && expected[i * 2 + 1] == hex[digest[i] & 15];
    }
    return ok;
}

// L4D1 campaigns use 0–3 for their original cast. This plugin puts their
// L4D2 guests in 4–7, which the stock credits interpret as the L4D1 cast.
constexpr int CreditsGuestIndex(int character, unsigned guests) noexcept {
    if (character < kStockLimit || character >= kLargestLimit) return -1;
    const int guest = character - kStockLimit;
    return (guests & (1u << guest)) ? guest : -1;
}

static_assert(CreditsGuestIndex(4, 0b0001) == 0 && CreditsGuestIndex(7, 0b1000) == 3);
static_assert(CreditsGuestIndex(4, 0b1000) == -1 && CreditsGuestIndex(6, 0) == -1);
static_assert(CreditsGuestIndex(-1, 15) == -1 && CreditsGuestIndex(0, 15) == -1 &&
              CreditsGuestIndex(3, 15) == -1 && CreditsGuestIndex(8, 15) == -1);

constexpr std::size_t kCreditsCharacter = 0x286530;
constexpr std::size_t kCreditsNameToken = 0x286580;
constexpr std::size_t kCreditsFallbackName = 0x286640;
using CreditNameFn = const char* (__cdecl*)(int);
using CreditCharacterFn = int (__cdecl*)(int);

int __cdecl ResolveCreditsCharacter(int character) {
    const int guest = CreditsGuestIndex(character, g_creditsCharacters.load());
    return guest >= 0 ? guest : reinterpret_cast<CreditCharacterFn>(g_creditsClient + kCreditsCharacter)(character);
}

const char* __cdecl ResolveCreditsName(int character) {
    constexpr const char* tokens[]{"#L4D360UI_Gambler", "#L4D360UI_Producer", "#L4D360UI_Coach", "#L4D360UI_Mechanic"};
    const int guest = CreditsGuestIndex(character, g_creditsCharacters.load());
    return guest >= 0 ? tokens[guest] : reinterpret_cast<CreditNameFn>(g_creditsClient + kCreditsNameToken)(character);
}

const char* __cdecl ResolveCreditsFallbackName(int character) {
    constexpr const char* names[]{"Nick", "Rochelle", "Coach", "Ellis"};
    const int guest = CreditsGuestIndex(character, g_creditsCharacters.load());
    return guest >= 0 ? names[guest] : reinterpret_cast<CreditNameFn>(g_creditsClient + kCreditsFallbackName)(character);
}

bool WriteCreditsCall(std::size_t call, std::uintptr_t target) {
    auto* const operand = g_creditsClient + call + 1;
    DWORD protection;
    if (!VirtualProtect(operand, sizeof(std::uint32_t), PAGE_EXECUTE_READWRITE, &protection)) return false;
    const auto displacement = static_cast<std::uint32_t>(target - reinterpret_cast<std::uintptr_t>(operand + 4));
    Write(operand, 0, displacement);
    DWORD ignored;
    VirtualProtect(operand, sizeof(displacement), protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), operand, sizeof(displacement));
    return true;
}

bool InstallCreditsNames() {
    // These calls belong only to CStatsCrawl. Its stat records and the shared
    // character helpers used during gameplay retain their original behavior.
    struct Call {
        std::size_t offset;
        std::size_t original;
        std::uintptr_t replacement;
    };
    const auto resolveName = reinterpret_cast<std::uintptr_t>(&ResolveCreditsName);
    const Call calls[]{
        {0x2C1757, kCreditsNameToken, resolveName},
        {0x2C2BCE, kCreditsNameToken, resolveName},
        {0x2C2E21, kCreditsNameToken, resolveName},
        {0x2C31AB, kCreditsNameToken, resolveName},
        {0x2C3201, kCreditsFallbackName, reinterpret_cast<std::uintptr_t>(&ResolveCreditsFallbackName)},
        {0x2C3325, kCreditsCharacter, reinterpret_cast<std::uintptr_t>(&ResolveCreditsCharacter)},
    };
    for (const auto& call : calls) {
        const auto expected = static_cast<std::uint32_t>(call.original - (call.offset + 5));
        if (g_creditsClient[call.offset] != 0xE8 ||
            Read<std::uint32_t>(g_creditsClient, call.offset + 1) != expected) return false;
    }

    for (std::size_t i = 0; i < std::size(calls); ++i) {
        if (WriteCreditsCall(calls[i].offset, calls[i].replacement)) continue;
        while (i > 0) {
            const auto& patched = calls[--i];
            WriteCreditsCall(patched.offset, reinterpret_cast<std::uintptr_t>(g_creditsClient + patched.original));
        }
        return false;
    }
    return true;
}

bool g_creditsInstalled = false;

void PrepareCreditsNames() {
    if (g_creditsAttempted) return;
    const auto client = GetModuleHandleW(L"client.dll");
    if (!client) return;
    g_creditsAttempted = true;
    if (MatchesHash(client, "c0a8e1e88f7312db11bcfd4b0ec574d224a34b2d9a615d6088b28335eeb4af1f")) {
        g_creditsClient = reinterpret_cast<std::uint8_t*>(client);
        g_creditsInstalled = InstallCreditsNames();
    }
    Log(g_creditsInstalled ? "Campaign credits name correction ready." :
                             "Campaign credits client did not match; kept stock credits.");
}

void* FindConVar(const char* name) {
    void* cvar = g_cvarFactory("VEngineCvar007", nullptr);
    if (!cvar) return nullptr;
    void* entry = Method<FindVarFn>(cvar, 12)(cvar, name);
    return entry ? Read<void*>(entry, kConVarParent) : nullptr;
}

std::uint8_t* LocalServer() {
    return g_engine + kGameServer;
}

bool HasLocalServer() {
    const auto* server = LocalServer();
    return Read<int>(server, kServerState) >= 2 && !Read<bool>(server, kDedicated);
}

bool IsCoop() {
    const auto* mode = FindConVar("mp_gamemode");
    const auto* name = mode ? Read<const char*>(mode, kConVarString) : nullptr;
    return name && std::strcmp(name, "coop") == 0;
}

bool LobbyRequestsEight(int nativeSlots) {
    if (nativeSlots == kLargestLimit) return true;
    // The engine copies Members/numSlots into the game server. The current
    // session preserves the lobby value when map setup has already reset it.
    void* framework = Read<void*>(g_engine, kMatchFramework);
    if (!framework) return false;
    using GetObjectFn = void*(__thiscall*)(void*);
    void* session = Method<GetObjectFn>(framework, 9)(framework);
    void* settings = session ? Method<GetObjectFn>(session, 1)(session) : nullptr;
    using GetIntFn = int(__thiscall*)(void*, const char*, int);
    const auto getInt = reinterpret_cast<GetIntFn>(g_engine + 0x287990);
    return settings && getInt(settings, "Members/numSlots", 0) == kLargestLimit;
}

int HumanConnections() {
    using CountFn = int(__thiscall*)(void*);
    return Method<CountFn>(LocalServer(), 2)(LocalServer()) -
           Method<CountFn>(LocalServer(), 4)(LocalServer());
}

struct Survivors {
    int count = 0;
    int humans = 0;
    int idleOwners = 0;
    int freeBotCount = 0;
    std::array<int, 64> freeBotIds{};
    std::array<int, 8> characters{};
    std::uint64_t slots = 0;
};

void* PlayerInfo(const void* player) {
    using GetPlayerInfoFn = void*(__thiscall*)(void*, void*);
    void* edict = Read<void*>(player, 0x28);
    return edict ? Method<GetPlayerInfoFn>(g_playerInfoManager, 0)(g_playerInfoManager, edict) : nullptr;
}

bool IsBot(void* info) {
    using IsFakeClientFn = bool(__thiscall*)(void*);
    return info && Method<IsFakeClientFn>(info, 11)(info);
}

Survivors GetSurvivors() {
    Survivors team;
    const auto* globals = Read<void*>(g_server, kGlobals);
    const int maxClients = globals ? Read<int>(globals, 0x14) : 0;
    if (maxClients < kStockLimit || maxClients > 64) return team;

    const auto byIndex = reinterpret_cast<PlayerByIndexFn>(g_server + 0x207270);
    using UserIdFn = int(__thiscall*)(void*);
    for (int index = 1; index <= maxClients; ++index) {
        const auto* player = byIndex(index);
        if (!player || Read<int>(player, 0x238) != kSurvivorTeam) continue;
        ++team.count;
        team.slots |= std::uint64_t{1} << (index - 1);
        void* info = PlayerInfo(player);
        if (!IsBot(info)) {
            ++team.humans;
        } else if (Read<int>(player, 0x4324) > 0) {
            ++team.idleOwners;
        } else {
            const auto id = Method<UserIdFn>(info, 1)(info);
            if (id > 0) team.freeBotIds[team.freeBotCount++] = id;
        }

        const int character = Read<int>(player, 0x2BB4);
        if (character >= 0 && character < static_cast<int>(team.characters.size())) ++team.characters[character];
    }
    return team;
}

void HideMenu() {
    g_menuContext.store(0);
    g_request.store(0);
    g_reduceBlocked.store(false);
}

void StopMap() {
    HideMenu();
    g_creditsCharacters.store(0);
    g_activeRules.store(nullptr);
    if (g_map.ownsServerSlots && Read<int>(LocalServer(), kServerSlots) == g_sessionLimit) {
        Write(LocalServer(), kServerSlots, g_map.originalServerSlots);
    }
    g_map = {};
    g_waitForActivation = false;
    g_playableLevel = false;
}

bool ApplyLimit(int requested, bool newChapter) {
    if (!g_map.rules || !g_survivorLimit || !HasLocalServer() ||
        requested < kStockLimit || requested > g_map.highest) return false;

    const int current = Read<int>(g_survivorLimit, kConVarInt);
    if (requested < current) {
        const auto team = GetSurvivors();
        const int excess = team.count - requested;
        if (HumanConnections() > requested || team.humans + team.idleOwners > requested || excess > team.freeBotCount) {
            g_reduceBlocked.store(true);
            Log("Kept %d slots because reducing to %d could remove an occupied seat.", current, requested);
            return false;
        }
        if (excess > 0) {
            void* engine = Read<void*>(g_server, kEngineServer);
            if (!engine) return false;
            using CommandFn = void(__thiscall*)(void*, const char*);
            for (int index = 0; index < excess; ++index) {
                char command[40];
                const int id = team.freeBotIds[team.freeBotCount - 1 - index];
                std::snprintf(command, sizeof(command), "kickid %d\n", id);
                Method<CommandFn>(engine, 0x94 / sizeof(void*))(engine, command);
            }
            g_map.reducingTo = requested;
            g_map.reduceDeadline = GetTickCount64() + 5000;
            g_map.nextCheck = GetTickCount64() + 100;
            g_map.checksLeft = 0;
            g_reduceBlocked.store(false);
            Log("Queued removal of %d spare bots before setting the host limit to %d.", excess, requested);
            return true;
        }
    }

    const auto previousMax = Read<float>(g_survivorLimit, kConVarMax);
    if (requested > previousMax) Write(g_survivorLimit, kConVarMax, static_cast<float>(requested));
    if (current != requested) Method<SetIntFn>(g_survivorLimit, 14)(g_survivorLimit, requested);
    if (Read<int>(g_survivorLimit, kConVarInt) != requested) {
        Write(g_survivorLimit, kConVarMax, previousMax);
        Log("Engine rejected slot limit %d.", requested);
        return false;
    }

    Write(g_survivorLimit, kConVarMax, static_cast<float>(requested));
    if (requested > kStockLimit || g_map.originalServerSlots == kLargestLimit) {
        Write(LocalServer(), kServerSlots, requested);
        g_map.ownsServerSlots = true;
    } else if (g_map.ownsServerSlots) {
        Write(LocalServer(), kServerSlots, g_map.originalServerSlots);
        g_map.ownsServerSlots = false;
    }

    g_sessionLimit = requested;
    g_effective.store(requested);
    g_activeRules.store(requested > kStockLimit ? g_map.rules : nullptr);
    g_reduceBlocked.store(false);
    g_map.nextCheck = GetTickCount64() + (newChapter ? 1500 : 150);
    g_map.checksLeft = 80;
    Log("Host limit %d applied (%s).", requested, newChapter ? "map activation" : "L4N menu");
    return true;
}

void PrepareHostedMap() {
    if (!IsCoop()) return;
    void* rules = Read<void*>(g_server, kGameRules);
    const auto stockLimit = reinterpret_cast<RulesLimitFn>(g_server + 0x2DB060);
    const auto* globals = Read<void*>(g_server, kGlobals);
    const int physical = Read<int>(LocalServer(), kMaxClients);
    const int capacity = globals ? Read<int>(globals, 0x14) : 0;
    const int highest = (std::min)(kLargestLimit, (std::min)(physical, capacity));
    if (!rules || stockLimit(rules) != kStockLimit || highest < kStockLimit || !g_playerInfoManager) return;

    void* limit = FindConVar("survivor_limit");
    if (!limit || !Read<bool>(limit, kConVarHasMax) ||
        Read<float>(limit, kConVarMax) != g_sessionLimit ||
        Read<int>(limit, kConVarInt) != g_sessionLimit) {
        Log("Hosted map eligibility check failed for survivor_limit.");
        return;
    }

    const bool newSession = !g_survivorLimit;
    g_survivorLimit = limit;
    g_map.rules = rules;
    g_map.highest = highest;
    g_map.survivorSet = reinterpret_cast<int(__cdecl*)()>(g_server + 0x2DBB60)();
    g_map.originalServerSlots = Read<int>(LocalServer(), kServerSlots);
    const bool eightSlotLobby = newSession && LobbyRequestsEight(g_map.originalServerSlots);
    const int initial = eightSlotLobby ? kLargestLimit : g_sessionLimit;
    if (!ApplyLimit((std::min)(initial, highest), true)) {
        StopMap();
        return;
    }
    if (g_map.survivorSet == 1 && highest > kStockLimit) PrepareCreditsNames();
    if (eightSlotLobby) Log("Eight-seat lobby detected; applied a host limit of %d.", g_sessionLimit);
    g_mapEpoch = (g_mapEpoch % 0xFFFFFFu) + 1;
    g_menuContext.store((g_mapEpoch << 8) | static_cast<unsigned>(highest));
    Log("Local host menu enabled (4-%d), campaign survivor set %d.", highest, g_map.survivorSet);
}

int __fastcall RulesLimit(void* self, void*) {
    if (g_activeRules.load() == self) return g_effective.load();
    return reinterpret_cast<RulesLimitFn>(g_server + 0x2DB060)(self);
}

bool __fastcall LevelInit(void* self, void*, const char* map, const char* entities, const char* oldMap,
                          const char* landmark, bool loadGame, bool background) {
    const bool levelReady = reinterpret_cast<LevelInitFn>(g_server + 0xF21B0)(
        self, map, entities, oldMap, landmark, loadGame, background);
    g_playableLevel = levelReady && !background;
    return levelReady;
}

void __fastcall ServerActivate(void* self, void*, void* edicts, int count, int maxClients) {
    reinterpret_cast<ActivateFn>(g_server + 0xEF3D0)(self, edicts, count, maxClients);
    g_waitForActivation = g_playableLevel && !Read<bool>(LocalServer(), kDedicated);
}

struct Character {
    const char* name;
    const char* model;
    const char* responseContext;
};

constexpr std::array<Character, 4> kFirstGame{{
    {"Bill", "models/survivors/survivor_namvet.mdl", "who:NamVet:0"},
    {"Zoey", "models/survivors/survivor_teenangst.mdl", "who:TeenGirl:0"},
    {"Francis", "models/survivors/survivor_biker.mdl", "who:Biker:0"},
    {"Louis", "models/survivors/survivor_manager.mdl", "who:Manager:0"},
}};

constexpr std::array<Character, 4> kSecondGame{{
    {"Nick", "models/survivors/survivor_gambler.mdl", "who:Gambler:0"},
    {"Rochelle", "models/survivors/survivor_producer.mdl", "who:Producer:0"},
    {"Coach", "models/survivors/survivor_coach.mdl", "who:Coach:0"},
    {"Ellis", "models/survivors/survivor_mechanic.mdl", "who:Mechanic:0"},
}};

void SetExtraCharacter(std::uint64_t newSlots, const Survivors& before) {
    if (!newSlots || (newSlots & (newSlots - 1)) || (g_map.survivorSet != 1 && g_map.survivorSet != 2)) return;
    const auto available = std::find(before.characters.begin() + 4, before.characters.end(), 0);
    if (available == before.characters.end()) return;
    const int identity = static_cast<int>(available - before.characters.begin());
    const Character& character = (g_map.survivorSet == 1 ? kSecondGame : kFirstGame)[identity - 4];
    int index = 1;
    for (; (newSlots & 1) == 0; newSlots >>= 1) ++index;
    const auto byIndex = reinterpret_cast<PlayerByIndexFn>(g_server + 0x207270);
    void* bot = byIndex(index);
    if (!bot || Read<int>(bot, 0x238) != kSurvivorTeam || Read<int>(bot, 0x4324) > 0 || !IsBot(PlayerInfo(bot))) return;

    void* engine = Read<void*>(g_server, kEngineServer);
    if (!engine) return;
    using IsPrecachedFn = bool(__thiscall*)(void*, const char*);
    using PrecacheFn = int(__thiscall*)(void*, const char*, bool);
    if (!Method<IsPrecachedFn>(engine, 9)(engine, character.model) &&
        Method<PrecacheFn>(engine, 5)(engine, character.model, true) <= 0) {
        Log("Model not available for extra survivor %s; retained the stock bot.", character.name);
        return;
    }

    using SetModelFn = void(__thiscall*)(void*, const char*);
    using SetCharacterFn = void(__thiscall*)(void*, int);
    using SetFakeClientValueFn = void(__thiscall*)(void*, void*, const char*, const char*);
    using NetworkChangedFn = void(__thiscall*)(void*, void*);
    constexpr std::size_t playerClass = 0x2918;
    if (Read<int>(bot, playerClass) != identity + 1) {
        Method<NetworkChangedFn>(bot, 0x20C / sizeof(void*))(bot, static_cast<std::uint8_t*>(bot) + playerClass);
        Write(bot, playerClass, identity + 1);
    }
    Method<SetModelFn>(bot, 27)(bot, character.model);
    reinterpret_cast<SetCharacterFn>(g_server + 0x3164E0)(bot, identity);
    reinterpret_cast<SetModelFn>(g_server + 0x63D00)(bot, character.responseContext);
    if (g_map.survivorSet == 1) {
        Method<SetFakeClientValueFn>(engine, 0x15C / sizeof(void*))(engine, Read<void*>(bot, 0x28), "name", character.name);
    }

    using GetModelFn = const char*(__thiscall*)(void*);
    void* info = PlayerInfo(bot);
    const char* model = info ? Method<GetModelFn>(info, 20)(info) : nullptr;
    if (model && std::strcmp(model, character.model) == 0 && Read<int>(bot, 0x2BB4) == identity) {
        if (g_map.survivorSet == 1 && g_creditsInstalled) {
            g_creditsCharacters.fetch_or(1u << (identity - kStockLimit));
        }
        Log("Extra survivor %s ready (identity %d, survivor set %d, team %d, model %s).", character.name,
            identity, g_map.survivorSet, Read<int>(bot, 0x238), model);
    } else {
        Log("Could not verify the final character and model for extra survivor %s.", character.name);
    }
}

void UpdateRoster() {
    auto team = GetSurvivors();
    if (team.count < g_sessionLimit && g_sessionLimit > kStockLimit &&
        team.count >= kStockLimit && HumanConnections() > 0) {
        void* director = Read<void*>(g_server, kDirector);
        if (director) {
            const auto fewest = std::min_element(team.characters.begin(), team.characters.begin() + 4);
            const int character = static_cast<int>(fewest - team.characters.begin());
            reinterpret_cast<AddSurvivorFn>(g_server + 0x26CC60)(director, character);
            const auto after = GetSurvivors();
            SetExtraCharacter(after.slots & ~team.slots, team);
            team = after;
        }
    }
    if (team.count == g_sessionLimit || !g_map.checksLeft) {
        Log("Host roster %d survivors, %d connected humans, survivor_limit=%d, server slots=%d.",
            team.count, HumanConnections(), Read<int>(g_survivorLimit, kConVarInt),
            Read<int>(LocalServer(), kServerSlots));
        g_map.checksLeft = 0;
    }
}

void __fastcall GameFrame(void* self, void*, bool simulating) {
    reinterpret_cast<FrameFn>(g_server + 0xEF4B0)(self, simulating);
    if (g_waitForActivation && HasLocalServer()) {
        g_waitForActivation = false;
        PrepareHostedMap();
    }
    if (!g_map.rules || !HasLocalServer()) return;

    if (const unsigned request = g_request.exchange(0)) {
        const auto context = g_menuContext.load();
        if ((request >> 8) == (context >> 8) && IsCoop()) {
            g_map.reducingTo = 0;
            ApplyLimit(request & 0xFFu, false);
        }
    }
    if (g_map.reducingTo) {
        const auto now = GetTickCount64();
        if (now < g_map.nextCheck) return;
        g_map.nextCheck = now + 100;
        if (GetSurvivors().count <= g_map.reducingTo) {
            const int limit = g_map.reducingTo;
            g_map.reducingTo = 0;
            if (IsCoop() && ApplyLimit(limit, false)) return;
        } else if (now >= g_map.reduceDeadline) {
            g_map.reducingTo = 0;
            Log("Could not confirm the spare bots were removed; kept the current host limit.");
        } else {
            return;
        }
        // Keep the previous roster usable if only some kicks succeeded.
        g_map.nextCheck = now + 350;
        g_map.checksLeft = 80;
        return;
    }
    if (!simulating || !g_map.checksLeft) return;
    const auto now = GetTickCount64();
    if (now < g_map.nextCheck) return;
    g_map.nextCheck = now + 350;
    --g_map.checksLeft;
    UpdateRoster();
}

void __fastcall LevelShutdown(void* self, void*) {
    StopMap();
    reinterpret_cast<ShutdownFn>(g_server + 0xEDB40)(self);
}

void __fastcall GameShutdown(void* self, void*) {
    StopMap();
    if (g_survivorLimit && g_sessionLimit != kStockLimit) {
        if (Read<float>(g_survivorLimit, kConVarMax) == g_sessionLimit) {
            Write(g_survivorLimit, kConVarMax, static_cast<float>(kStockLimit));
            if (Read<int>(g_survivorLimit, kConVarInt) == g_sessionLimit) {
                Method<SetIntFn>(g_survivorLimit, 14)(g_survivorLimit, kStockLimit);
            }
        }
        Log("Local host session ended; restored the stock survivor limit.");
    }
    g_survivorLimit = nullptr;
    g_sessionLimit = kStockLimit;
    g_effective.store(kStockLimit);
    reinterpret_cast<ShutdownFn>(g_server + 0xEF2C0)(self);
}
struct Patch {
    void** slot;
    void* original;
    void* replacement;
};

bool Swap(const Patch& patch, bool install) {
    DWORD protection;
    if (!VirtualProtect(patch.slot, sizeof(void*), PAGE_READWRITE, &protection)) return false;
    auto* expected = install ? patch.original : patch.replacement;
    auto* desired = install ? patch.replacement : patch.original;
    const auto previous = InterlockedCompareExchangePointer(patch.slot, desired, expected);
    DWORD ignored;
    VirtualProtect(patch.slot, sizeof(void*), protection, &ignored);
    return previous == expected;
}

bool InstallHooks() {
    auto** game = reinterpret_cast<void**>(g_server + 0x60C904);
    auto** rules = reinterpret_cast<void**>(g_server + 0x68E9D4);
    const Patch patches[] = {
        {rules + 136, g_server + 0x2DB060, reinterpret_cast<void*>(&RulesLimit)},
        {game + 7, g_server + 0xEF2C0, reinterpret_cast<void*>(&GameShutdown)},
        {game + 6, g_server + 0xEDB40, reinterpret_cast<void*>(&LevelShutdown)},
        {game + 4, g_server + 0xEF4B0, reinterpret_cast<void*>(&GameFrame)},
        {game + 3, g_server + 0xEF3D0, reinterpret_cast<void*>(&ServerActivate)},
        {game + 2, g_server + 0xF21B0, reinterpret_cast<void*>(&LevelInit)},
    };
    for (const auto& patch : patches) {
        if (*patch.slot != patch.original) return false;
    }
    for (std::size_t i = 0; i < std::size(patches); ++i) {
        if (Swap(patches[i], true)) continue;
        while (i) Swap(patches[--i], false);
        return false;
    }
    return true;
}

DWORD WINAPI Start(void*) {
    for (int attempt = 0; attempt < 1200; ++attempt) {
        const auto engine = GetModuleHandleW(L"engine.dll");
        const auto server = GetModuleHandleW(L"server.dll");
        const auto cvar = GetModuleHandleW(L"vstdlib.dll");
        if (engine && server && cvar) {
            if (!MatchesHash(engine, "8a6d468059394bf4cfce32f11dcba12ef45de88da1373084d5a2916d0bf7ba86") ||
                !MatchesHash(server, "a93ec916a8f26fbea85688c321d2ba8dd346d435fab09a0b926dd950c2e1ea12") ||
                !MatchesHash(cvar, "37d0c98ddfe7fa199d741dfc24526eb435f71f66d2d0e3d0500c4c1be33cb6c5")) {
                Log("Module hash mismatch. Kept stock game behavior.");
                return 0;
            }
            g_engine = reinterpret_cast<std::uint8_t*>(engine);
            g_server = reinterpret_cast<std::uint8_t*>(server);
#pragma warning(suppress : 4191)
            g_cvarFactory = reinterpret_cast<FactoryFn>(GetProcAddress(cvar, "CreateInterface"));
#pragma warning(suppress : 4191)
            const auto serverFactory = reinterpret_cast<FactoryFn>(GetProcAddress(server, "CreateInterface"));
            g_playerInfoManager = serverFactory ? serverFactory("PlayerInfoManager002", nullptr) : nullptr;
            Log(g_cvarFactory && g_playerInfoManager && InstallHooks() ? "Waiting for a locally hosted campaign." :
                                                                       "Engine entry points did not match.");
            return 0;
        }
        Sleep(50);
    }
    Log("Timed out waiting for the game server modules.");
    return 0;
}

constexpr char kTitle[] = u8"로컬 협동 인원수";

const char* __cdecl SelectLimit(void* data) noexcept;

const char* BuildMenu(unsigned context) noexcept {
    if (!context) return nullptr;
    static thread_local char menu[640];
    const auto callback = static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(&SelectLimit));
    const int current = g_effective.load();
    const char* status = g_reduceBlocked.load() ? u8" / 사용 중인 자리 있음" : "";
    int used = std::snprintf(menu, sizeof(menu), u8"\"%s (%d명%s)\" { \"_refresh\" \"1\" ", kTitle, current, status);
    for (int limit = kStockLimit; limit <= static_cast<int>(context & 0xFFu); ++limit) {
        const auto action = (context & ~0xFFu) | static_cast<unsigned>(limit);
        if (used < 0 || static_cast<std::size_t>(used) >= sizeof(menu) - 2) return nullptr;
        const int written = std::snprintf(menu + used, sizeof(menu) - used,
            u8"\"%d명\" { \"callback\" \"0x%X\" \"user_data\" \"0x%X\" } ", limit, callback, action);
        if (written < 0 || static_cast<std::size_t>(written) >= sizeof(menu) - used) return nullptr;
        used += written;
    }
    if (used < 0 || static_cast<std::size_t>(used) >= sizeof(menu) - 1) return nullptr;
    menu[used++] = '}';
    menu[used] = '\0';
    return menu;
}

const char* __cdecl SelectLimit(void* data) noexcept {
    const auto action = static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(data));
    const unsigned context = g_menuContext.load();
    if (!context || (action >> 8) != (context >> 8)) return nullptr;
    const auto requested = action & 0xFFu;
    if (requested >= kStockLimit && requested <= (context & 0xFFu)) {
        g_request.store(action);
        Log("L4N menu selected %u slots.", requested);
    }
    return BuildMenu(context);
}

class Plugin final : public IL4NPlugin {
public:
    const char* GetName() override { return "NekoLocalCoopSlots"; }
    const char* GetVersion() override { return "0.1.2-test"; }

    void OnGameLaunch() override {
        if (g_starting.exchange(true)) return;
        HMODULE pinned = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                                reinterpret_cast<LPCWSTR>(&g_starting), &pinned)) return;
        const auto thread = CreateThread(nullptr, 0, &Start, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }

    const char* RequestHudMenu(bool title) override {
        const unsigned context = g_menuContext.load();
        if (title) {
            const int visible = context ? 1 : 0;
            if (g_lastMenuVisibility.exchange(visible) != visible) {
                Log(visible ? "L4N menu title offered to the host." : "L4N menu title hidden outside a hosted campaign.");
            }
            return context ? kTitle : nullptr;
        }
        return BuildMenu(context);
    }
};

}  // namespace

extern "C" __declspec(dllexport) IL4NPlugin* GetL4NPluginInstance() {
    static Plugin instance;
    return &instance;
}

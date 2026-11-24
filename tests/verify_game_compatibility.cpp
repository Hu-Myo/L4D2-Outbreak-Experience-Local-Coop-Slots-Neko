#include "../plugin.cpp"

#include <cassert>
#include <filesystem>
#include <fstream>

namespace {
namespace fs = std::filesystem;

class Image {
public:
    explicit Image(const fs::path& path) : module_(LoadLibraryExW(path.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES)) {
        assert(module_);
    }
    ~Image() { FreeLibrary(module_); }
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    operator HMODULE() const { return module_; }

private:
    HMODULE module_;
};

class ScopedByteChange {
public:
    ScopedByteChange(HMODULE module, std::size_t rva)
        : location_(reinterpret_cast<std::uint8_t*>(module) + rva), original_(Read<std::uint8_t>(module, rva)) {
        Set(original_ ^ 1u);
    }
    ~ScopedByteChange() { Set(original_); }
    ScopedByteChange(const ScopedByteChange&) = delete;
    ScopedByteChange& operator=(const ScopedByteChange&) = delete;

private:
    void Set(std::uint8_t value) {
        DWORD protection;
        assert(VirtualProtect(location_, 1, PAGE_EXECUTE_READWRITE, &protection));
        *location_ = value;
        DWORD ignored;
        assert(VirtualProtect(location_, 1, protection, &ignored));
    }

    std::uint8_t* location_;
    std::uint8_t original_;
};

class TestCopies {
public:
    TestCopies() {
        wchar_t temp[MAX_PATH]{}, unique[MAX_PATH]{};
        assert(GetTempPathW(MAX_PATH, temp) != 0);
        assert(GetTempFileNameW(temp, L"ncs", 0, unique) != 0);
        assert(DeleteFileW(unique));
        folder = unique;
        assert(fs::create_directory(folder));
    }
    ~TestCopies() {
        for (const auto* name : {L"engine-fixture.dll", L"server-fixture.dll", L"vstdlib-fixture.dll", L"client-fixture.dll"}) {
            fs::remove(folder / name);
        }
        fs::remove(folder);
    }
    TestCopies(const TestCopies&) = delete;
    TestCopies& operator=(const TestCopies&) = delete;

    fs::path Add(const fs::path& source, const wchar_t* name) const {
        const auto copy = folder / name;
        fs::copy_file(source, copy);
        // Change the file digest without changing mapped sections.
        std::ofstream file(copy, std::ios::binary | std::ios::app);
        file.put('\0');
        assert(file);
        return copy;
    }

private:
    fs::path folder;
};

std::uintptr_t CallTarget(const std::uint8_t* image, std::size_t offset) {
    const auto* const next = image + offset + 5;
    return reinterpret_cast<std::uintptr_t>(next) + Read<std::uint32_t>(next - 4, 0);
}

void VerifyHooks() {
    auto** game = reinterpret_cast<void**>(g_server + g_build->server.gameVtable);
    auto** rules = reinterpret_cast<void**>(g_server + g_build->server.rulesVtable);
    assert(InstallHooks());
    assert(game[2] == reinterpret_cast<void*>(&LevelInit));
    assert(game[3] == reinterpret_cast<void*>(&ServerActivate));
    assert(game[4] == reinterpret_cast<void*>(&GameFrame));
    assert(game[6] == reinterpret_cast<void*>(&LevelShutdown));
    assert(game[7] == reinterpret_cast<void*>(&GameShutdown));
    assert(rules[136] == reinterpret_cast<void*>(&RulesLimit));

    assert(IsPassingChapter("c6m1_riverbank"));
    assert(IsPassingChapter("C6M3_PORT"));
    assert(!IsPassingChapter("c6m2_bedlam") && !IsPassingChapter(nullptr));
    assert(!IsPassingNpcIdentity(4) && !IsPassingNpcIdentity(8));
    assert(IsPassingNpcIdentity(5) && IsPassingNpcIdentity(6) && IsPassingNpcIdentity(7));

    assert(InstallPassingNpcRouting());
    const auto& passing = g_build->passing;
    assert(CallTarget(g_server, passing.spawnNpcCall) == reinterpret_cast<std::uintptr_t>(&SpawnPassingNpc));
    assert(CallTarget(g_server, passing.duplicateCheckCall) == reinterpret_cast<std::uintptr_t>(&FindExistingSpawnNpc));
    assert(CallTarget(g_server, passing.spawnedNpcLookupCall) == reinterpret_cast<std::uintptr_t>(&FindPassingMapTarget));
    assert(CallTarget(g_server, passing.namedTargetLookupCall) == reinterpret_cast<std::uintptr_t>(&FindPassingMapTarget));

    assert(InstallCreditsNames());
    for (const auto call : g_build->credits.nameCalls) {
        assert(CallTarget(g_creditsClient, call) == reinterpret_cast<std::uintptr_t>(&ResolveCreditsName));
    }
    assert(CallTarget(g_creditsClient, g_build->credits.fallbackCall) ==
           reinterpret_cast<std::uintptr_t>(&ResolveCreditsFallbackName));
    assert(CallTarget(g_creditsClient, g_build->credits.characterCall) ==
           reinterpret_cast<std::uintptr_t>(&ResolveCreditsCharacter));
    g_creditsCharacters = 15;
    constexpr const char* names[]{"Nick", "Rochelle", "Coach", "Ellis"};
    for (int i = 0; i < 4; ++i) {
        assert(ResolveCreditsCharacter(4 + i) == i);
        assert(std::strcmp(ResolveCreditsFallbackName(4 + i), names[i]) == 0);
    }
}
}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::fputs("Pass the Windows Left 4 Dead 2 installation folder for Steam build 26770911.\n", stderr);
        return 2;
    }
    const fs::path root(argv[1]);
    const auto enginePath = root / L"bin/engine.dll";
    const auto serverPath = root / L"left4dead2/bin/server.dll";
    const auto cvarPath = root / L"bin/vstdlib.dll";
    const auto clientPath = root / L"left4dead2/bin/client.dll";
    const Image originalEngine(enginePath), originalServer(serverPath);
    const auto known = FindGameBuild(originalEngine, originalServer);
    assert(known.profile == &coop_slots::kSteam26770911);
    assert(!known.structuralEngine && !known.structuralServer);

    const TestCopies copies;
    const Image engine(copies.Add(enginePath, L"engine-fixture.dll"));
    const Image server(copies.Add(serverPath, L"server-fixture.dll"));
    const Image cvar(copies.Add(cvarPath, L"vstdlib-fixture.dll"));
    const Image client(copies.Add(clientPath, L"client-fixture.dll"));
    assert(static_cast<HMODULE>(engine) != static_cast<HMODULE>(originalEngine));
    assert(static_cast<HMODULE>(server) != static_cast<HMODULE>(originalServer));
    assert(!MatchesHash(engine, known.profile->engine.hash));
    assert(!MatchesHash(server, known.profile->server.hash));
    assert(!MatchesHash(cvar, kCvarHash));
    assert(!MatchesHash(client, known.profile->credits.hash));
    assert(!FindGameBuild(engine, cvar).profile);

    auto current = FindGameBuild(engine, originalServer);
    assert(current.profile == known.profile && current.structuralEngine && !current.structuralServer);
    current = FindGameBuild(originalEngine, server);
    assert(current.profile == known.profile && !current.structuralEngine && current.structuralServer);
    {
        const ScopedByteChange unrelatedEngineCode(engine, 0x1000);
        current = FindGameBuild(engine, server);
        assert(current.profile == known.profile && current.structuralEngine && current.structuralServer);
    }
    {
        const ScopedByteChange wrongEngineLayout(engine, 0x134662);
        assert(!FindGameBuild(engine, server).profile);
    }
    {
        const ScopedByteChange wrongPlayerLayout(server, 0x31661F);
        assert(!FindGameBuild(engine, server).profile);
    }
    {
        const ScopedByteChange wrongPassingContext(server, 0x29AACE);
        assert(!FindGameBuild(engine, server).profile);
    }
    g_build = current.profile;
    g_structuralServer = current.structuralServer;
    {
        const auto factory = GetProcAddress(cvar, "CreateInterface");
        assert(HasCompatibleCvar(cvar, factory, false));
        const ScopedByteChange changedCvar(cvar, 0x20B0);
        assert(!HasCompatibleCvar(cvar, factory, false));
    }
    {
        assert(SupportsCredits(client));
        const ScopedByteChange changedCreditsContext(client, 0x2C1815);
        assert(!SupportsCredits(client));
    }
    g_engine = reinterpret_cast<std::uint8_t*>(static_cast<HMODULE>(engine));
    g_server = reinterpret_cast<std::uint8_t*>(static_cast<HMODULE>(server));
    g_creditsClient = reinterpret_cast<std::uint8_t*>(static_cast<HMODULE>(client));
    VerifyHooks();
    std::puts("Verified reference and changed digests, ASLR, role-specific rejection, and all sixteen hook sites.");
}

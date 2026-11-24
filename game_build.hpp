#pragma once

#include <array>
#include <cstddef>

namespace coop_slots {

struct EngineOffsets {
    const char* hash;
    std::size_t localServer;
    std::size_t matchFramework;
    std::size_t keyValuesGetInt;
};

struct ServerOffsets {
    const char* hash;
    std::size_t gameRules;
    std::size_t director;
    std::size_t globals;
    std::size_t engineServer;
    std::size_t playerByIndex;
    std::size_t rulesLimit;
    std::size_t survivorSet;
    std::size_t levelInit;
    std::size_t serverActivate;
    std::size_t gameFrame;
    std::size_t levelShutdown;
    std::size_t gameShutdown;
    std::size_t setCharacter;
    std::size_t setResponseContext;
    std::size_t addSurvivor;
    std::size_t gameVtable;
    std::size_t rulesVtable;
};

struct CreditsOffsets {
    const char* hash;
    std::size_t character;
    std::size_t nameToken;
    std::size_t fallbackName;
    std::array<std::size_t, 4> nameCalls;
    std::size_t fallbackCall;
    std::size_t characterCall;
};

struct PassingOffsets {
    std::size_t characterLookup;
    std::size_t spawnNpcCall;
    std::size_t duplicateCheckCall;
    std::size_t spawnedNpcLookupCall;
    std::size_t namedTargetLookupCall;
};

struct GameBuild {
    const char* name;
    EngineOffsets engine;
    ServerOffsets server;
    CreditsOffsets credits;
    PassingOffsets passing;
};

inline constexpr GameBuild kSteam26770911{
    "Steam build 26770911",
    {"d451800729322f25bfc65d58a408edb4f4243a8ea8c26fabccbba4bc7ecc1db3", 0x645558, 0x6E0B14, 0x287B20},
    {"6c543884961f87b8a86cebfc980eabc8501b63387397f1ee583d9e32cecc9950",
     0x7F7F6C, 0x83B1B8, 0x7F7690, 0x7F7698,
     0x207400, 0x2DB160, 0x2DBBE0,
     0xF2260, 0xEF480, 0xEF560, 0xEDBF0, 0xEF370,
     0x316600, 0x63DA0, 0x26CDB0,
     0x60C904, 0x68EA2C},
    {"4d01ed9fb856d57ce549b8587c386f361083ee581b001295fac0eff4c835d52d",
     0x286600, 0x286650, 0x286710, {0x2C1827, 0x2C2C9E, 0x2C2EF1, 0x2C327B}, 0x2C32D1, 0x2C33F5},
    {0x305630, 0x29AADB, 0x26CEA8, 0x29AAE7, 0xB4F9E},
};

inline constexpr GameBuild kPreviousBuild{
    "previous Steam build",
    {"8a6d468059394bf4cfce32f11dcba12ef45de88da1373084d5a2916d0bf7ba86", 0x645558, 0x6E0B14, 0x287990},
    {"a93ec916a8f26fbea85688c321d2ba8dd346d435fab09a0b926dd950c2e1ea12",
     0x7F7F6C, 0x83B1B8, 0x7F7690, 0x7F7698,
     0x207270, 0x2DB060, 0x2DBB60,
     0xF21B0, 0xEF3D0, 0xEF4B0, 0xEDB40, 0xEF2C0,
     0x3164E0, 0x63D00, 0x26CC60,
     0x60C904, 0x68E9D4},
    {"c0a8e1e88f7312db11bcfd4b0ec574d224a34b2d9a615d6088b28335eeb4af1f",
     0x286530, 0x286580, 0x286640, {0x2C1757, 0x2C2BCE, 0x2C2E21, 0x2C31AB}, 0x2C3201, 0x2C3325},
    {},
};

}  // namespace coop_slots

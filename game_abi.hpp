#pragma once

#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace coop_slots {

struct AbiAnchor {
    std::size_t rva;
    std::size_t size;
    bool executable;
    std::size_t fixupCount;
    std::array<std::size_t, 16> fixups;
};

using Sha256 = std::array<UCHAR, 32>;

inline bool DigestMatches(const Sha256& digest, const char* expected) {
    constexpr char hex[] = "0123456789abcdef";
    for (std::size_t i = 0; i < digest.size(); ++i) {
        if (expected[2 * i] != hex[digest[i] >> 4] ||
            expected[2 * i + 1] != hex[digest[i] & 15]) return false;
    }
    return true;
}

enum class ImageAccess { Read, Code, Write };

inline bool HasImageAccess(HMODULE module, std::size_t rva, std::size_t size, ImageAccess access) {
    const auto base = reinterpret_cast<std::uintptr_t>(module);
    if (!module || !size || rva > UINTPTR_MAX - base || size > UINTPTR_MAX - base - rva) return false;
    const auto start = base + rva;
    const auto end = start + size;
    auto current = start;
    while (current < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<const void*>(current), &info, sizeof(info)) ||
            info.AllocationBase != module || info.Type != MEM_IMAGE || info.State != MEM_COMMIT ||
            (info.Protect & PAGE_GUARD)) return false;
        const auto protection = info.Protect & 0xff;
        const bool writable = protection == PAGE_READWRITE || protection == PAGE_WRITECOPY;
        const bool executable = protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE ||
                                protection == PAGE_EXECUTE_WRITECOPY;
        if (access == ImageAccess::Code ? !executable :
            access == ImageAccess::Write ? !writable : !writable && protection != PAGE_READONLY) return false;
        const auto blockBase = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
        if (info.RegionSize > UINTPTR_MAX - blockBase) return false;
        current = blockBase + info.RegionSize;
    }
    return true;
}

template <std::size_t Count>
bool MatchesAbi(HMODULE module, const AbiAnchor (&anchors)[Count], const char* expected) {
    // Windows can load different images at different bases. Canonicalize only
    // the known HIGHLOW operands, including vtable pointers, to module RVAs.
    // A moved or changed operand therefore changes the digest.
    const auto base = reinterpret_cast<std::uintptr_t>(module);
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    bool ok = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0 &&
              BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) == 0;
    std::array<UCHAR, 256> canonical;
    for (const auto& anchor : anchors) {
        if (!ok || anchor.size > canonical.size() || anchor.fixupCount > anchor.fixups.size() ||
            !HasImageAccess(module, anchor.rva, anchor.size, anchor.executable ? ImageAccess::Code : ImageAccess::Read)) {
            ok = false;
            break;
        }
        std::memcpy(canonical.data(), reinterpret_cast<const void*>(base + anchor.rva), anchor.size);
        for (std::size_t i = 0; ok && i < anchor.fixupCount; ++i) {
            const auto offset = anchor.fixups[i];
            std::uint32_t address;
            if (offset > anchor.size || sizeof(address) > anchor.size - offset) {
                ok = false;
                break;
            }
            std::memcpy(&address, canonical.data() + offset, sizeof(address));
            if (address < base) {
                ok = false;
                break;
            }
            address -= static_cast<std::uint32_t>(base);
            std::memcpy(canonical.data() + offset, &address, sizeof(address));
        }
        ok = ok && BCryptHashData(hash, canonical.data(), static_cast<ULONG>(anchor.size), 0) == 0;
    }
    Sha256 digest{};
    ok = ok && BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) == 0 &&
         DigestMatches(digest, expected);
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    return ok;
}

}  // namespace coop_slots

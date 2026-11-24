"""Generate ABI anchors from the verified, unmodified Steam build 26770911.

Requires pefile. The generator checks the source hashes before emitting anchors.
Run with the path to the reference Left 4 Dead 2 installation.
"""

from bisect import bisect_left
from hashlib import sha256
from pathlib import Path
from struct import pack_into, unpack_from
import sys

import pefile


ENGINE = (
    # Local server constructor, fields, and CGameServer virtual calls.
    (0x33EF00, 10), (0x1248C0, 18), (0x121DA0, 20),
    (0x37AF64 + 2 * 4, 4), (0x37AF64 + 4 * 4, 8), (0x37AF64 + 23 * 4, 4),
    (0x135150, 0x3B), (0x1350F0, 0x54), (0x134660, 7), (0xB810, 7),
    # Native lobby setting, framework calls, and KeyValues::GetInt.
    (0x121D6C, 0x18), (0x13C0A2, 0x1A), (0x37A168, 17),
    (0x297C75, 0x15), (0x3989B4, 19), (0x168A35, 0x2D), (0x287B20, 0x7C),
    # VEngineServer022 registration and the four methods the plugin uses.
    (0x33FCA0, 21), (0x1304C0, 6), (0x37E588, 17), (0x42A4FC, 4),
    (0x37D9FC + 5 * 4, 4), (0x37D9FC + 9 * 4, 4),
    (0x37D9FC + 37 * 4, 4), (0x37D9FC + 87 * 4, 4),
    (0x130890, 0x3E), (0x130910, 25), (0x130D60, 0x30), (0x132820, 0x32),
)

SERVER = (
    # ServerGameDLL005 registration, lifecycle methods, and survivor rules ctor.
    (0x5883D0, 21), (0x60D088, 17), (0xEDA00, 6), (0x76E398, 4), (0x60C904 + 2 * 4, 12),
    (0x60C904 + 6 * 4, 8), (0x68EA2C + 136 * 4, 4), (0x2E1E20, 0x29),
    (0xF2260, 0x43), (0xEF480, 0x31), (0xEF560, 0x43),
    (0xEDBF0, 0x43), (0xEF370, 0x42), (0x2DB160, 0x5A), (0x2DBBE0, 0x77),
    # Joins, the native survivor limit cvar, and the attached engine interface.
    (0x26CDB0, 0x40), (0x69ACD0, 15), (0x59C237, 0x20),
    (0x4FD630, 0x61), (0x4FD6A0, 0x65), (0x6FD4BC + 14 * 4, 4), (0x4FD060, 0xD6),
    (0xF16A3, 27), (0x60D2B4, 17), (0x207400, 0x61),
    # PlayerInfoManager002, player fields, and survivor bot method slots.
    (0x501DF0, 9), (0x501D80, 0x6A), (0x58BC70, 21), (0x62F090, 21), (0x1899D0, 6),
    (0x7829C0, 4), (0x62F044, 4), (0x189A40, 0x49), (0x62B5E4 + 4, 4),
    (0x62B5E4 + 11 * 4, 4), (0x62B5E4 + 20 * 4, 4),
    (0x16DD90, 24), (0x16B170, 13), (0x16B280, 0x21),
    (0x299580, 16), (0x301C40, 0x28), (0x2418E0, 7),
    (0x6B0E94 + 27 * 4, 4), (0x6B0E94 + 131 * 4, 4),
    (0x16E0F0, 0x30), (0x79CB0, 0x28), (0x316600, 0x67), (0x63DA0, 0x42),
)

VSTDLIB = (
    # VEngineCvar007's factory, registered CCvar, and FindVar slots.
    (0x10D30, 9), (0x10CC0, 0x6A), (0x1F500, 21), (0x1FB0, 6), (0x20410, 15),
    (0x207AC + 10 * 4, 4), (0x207AC + 12 * 4, 4), (0x20B0, 0x35), (0x3B00, 0x25),
)

PASSING = (
    # The separate map spawner, duplicate check, and !character target lookup.
    (0x305630, 0x37), (0x29AACE, 0x34), (0x26CE9F, 0x25), (0xB4F96, 0x20),
)

CREDITS = (
    # Name/identity helpers and all six calls in the credits renderer.
    (0x286600, 0x36), (0x286650, 0x50), (0x286710, 0x49),
    (0x2C1815, 0x2C), (0x2C2C87, 0x29), (0x2C2EE5, 0x28),
    (0x2C326F, 0x2A), (0x2C32C2, 0x28), (0x2C33E3, 0x27),
)

REFERENCE = (
    ("Engine", "bin/engine.dll", "d451800729322f25bfc65d58a408edb4f4243a8ea8c26fabccbba4bc7ecc1db3", ENGINE),
    ("Server", "left4dead2/bin/server.dll", "6c543884961f87b8a86cebfc980eabc8501b63387397f1ee583d9e32cecc9950", SERVER),
    ("Cvar", "bin/vstdlib.dll", "37d0c98ddfe7fa199d741dfc24526eb435f71f66d2d0e3d0500c4c1be33cb6c5", VSTDLIB),
    ("Passing", "left4dead2/bin/server.dll", "6c543884961f87b8a86cebfc980eabc8501b63387397f1ee583d9e32cecc9950", PASSING),
    ("Credits", "left4dead2/bin/client.dll", "4d01ed9fb856d57ce549b8587c386f361083ee581b001295fac0eff4c835d52d", CREDITS),
)


def emit_image(root, name, relative_path, reference_hash, regions):
    data = (root / relative_path).read_bytes()
    if sha256(data).hexdigest() != reference_hash:
        raise ValueError(f"{relative_path} does not match the reference build")
    image = pefile.PE(data=data)
    base = image.OPTIONAL_HEADER.ImageBase
    fixups = sorted(entry.rva for block in image.DIRECTORY_ENTRY_BASERELOC
                    for entry in block.entries if entry.type == pefile.RELOCATION_TYPE["IMAGE_REL_BASED_HIGHLOW"])
    normalized = sha256()
    print(f"inline constexpr AbiAnchor k{name}Anchors[]{{")
    for rva, size in regions:
        section = image.get_section_by_rva(rva)
        if image.get_section_by_rva(rva + size - 1) != section:
            raise ValueError(f"section boundary at {rva:#x}")
        canonical = bytearray(image.get_data(rva, size))
        assert len(canonical) == size
        affected = fixups[bisect_left(fixups, rva - 3):bisect_left(fixups, rva + size)]
        span_fixups = [fixup - rva for fixup in affected]
        if any(offset < 0 or offset + 4 > size for offset in span_fixups):
            raise ValueError(f"partial relocation at {rva:#x}, size {size:#x}: {span_fixups}")
        for offset in span_fixups:
            pointer, = unpack_from("<I", canonical, offset)
            if not base <= pointer < base + image.OPTIONAL_HEADER.SizeOfImage:
                raise ValueError(f"non-image pointer at {rva + offset:#x}")
            pack_into("<I", canonical, offset, pointer - base)
        normalized.update(canonical)
        code = "true" if section.Characteristics & 0x20000000 else "false"
        fixup_source = ", ".join(f"{fixup:#x}" for fixup in span_fixups)
        if len(span_fixups) > 16 or size > 256:
            raise ValueError(f"oversized anchor at {rva:#x}")
        print(f"    {{0x{rva:X}, {size:#x}, {code}, {len(span_fixups)}, {{{fixup_source}}}}},")
    print(f'}};\ninline constexpr char k{name}Digest[] = "{normalized.hexdigest()}";\n')


if __name__ == "__main__":
    print("// Generated by tests/generate_abi_anchors.py from Steam build 26770911.")
    print("#pragma once\n\nnamespace coop_slots {\n")
    for entry in REFERENCE:
        emit_image(Path(sys.argv[1]), *entry)
    print("}  // namespace coop_slots")

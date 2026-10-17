"""Pre-place Scooby-Doo! Unmasked's runtime-loaded logic code so it can be statically translated.

The game loads compiled logic modules (PowerPC relocatable ELF objects embedded in each level's .ghr) with
EFLogicCode::LoadFile (0x800E799C): MMMalloc the .text, memcpy it, apply .rela.text, call it. Heap addresses
change between runs, so the code can't be translated where the game puts it. Instead every unique module gets a
fixed address in the overlay region above MEM1 (0x81800000, runtime memory.cpp MEM1_LOGIC_OVERLAY); this tool
relocates each exactly as the game's loader would and appends the result to main.dol as text section 2, and the
loader's MMMalloc/MMFree calls are patched to stubs (hle/scooby_logic.cpp) that return that fixed address for a
module recognised by its key. The game then relocates into the same place and produces the same bytes.

Module key: FNV-1a 64 of the raw .text bytes followed by the raw .rela.text bytes (computed again at runtime).

Usage: build_logic_overlay.py <Disc/DATA/files> <game/main.dol> <out game/main_logic.dol> <out MAP_logic.txt>
                              <out runtime table .inc> [--levels A,B,...]
--levels limits the MAP entries (what gets translated) to modules used by those levels; addresses never change.
"""
import os, struct, sys

OVERLAY_BASE = 0x81800000
OVERLAY_SIZE = 0x01800000
STUB_ALLOC = OVERLAY_BASE + 0x00    # LogicAlloc stub (bl from 0x800E7AE8)
STUB_FREE = OVERLAY_BASE + 0x08     # LogicFree stub (bl from 0x800E796C)
# Modules high in the overlay are more than 32 MiB from memcpy/memset, out of bl range; their calls go through
# these veneers (b memcpy / b memset). Only the translation sees this: translated code follows this image, not the
# bytes the game's loader writes at run time.
VENEER_MEMCPY = OVERLAY_BASE + 0x10
VENEER_MEMSET = OVERLAY_BASE + 0x18
FIRST_MODULE = OVERLAY_BASE + 0x40
MEMCPY, MEMSET = 0x80011B78, 0x80011A8C
CALL_ALLOC, CALL_FREE = 0x800E7AE8, 0x800E796C  # bl MMMalloc / bl MMFree
MMMALLOC, MMFREE = 0x80070AA0, 0x800707BC
DEADFEED = 0xDEADFEED

def fnv1a64(data, h=0xCBF29CE484222325):
    for b in data:
        h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h

def parse(data, i):
    shoff, = struct.unpack('>I', data[i + 32:i + 36]); shnum, shstr = struct.unpack('>HH', data[i + 48:i + 52])
    secs = [struct.unpack('>10I', data[i + shoff + k * 40:i + shoff + k * 40 + 40]) for k in range(shnum)]
    so = i + secs[shstr][4]
    name = lambda o, base=so: data[base + o:data.index(b'\0', base + o)]
    by = {name(s[0]): s for s in secs}
    get = lambda s: data[i + s[4]:i + s[4] + s[5]]
    text, rela, sym, strt = by[b'.text'], by[b'.rela.text'], by[b'.symtab'], by[b'.strtab']
    strtab = get(strt)
    syms = []
    symdata = get(sym)
    for k in range(len(symdata) // 16):
        st_name, st_value, st_size, st_info, _o, st_shndx = struct.unpack('>IIIBBH', symdata[k * 16:k * 16 + 16])
        syms.append((strtab[st_name:strtab.index(b'\0', st_name)].decode('latin1'), st_value, st_info, st_shndx))
    return get(text), get(rela), syms

def relocate(text, rela, syms, base):
    """EFLogicCode::LoadFile's relocation loop (0x800E7B4C..0x800E7DA4), bit for bit."""
    out = bytearray(text)
    for k in range(len(rela) // 12):
        r_offset, r_info, r_addend = struct.unpack('>IIi', rela[k * 12:k * 12 + 12])
        rtype, sidx = r_info & 0xFF, r_info >> 8
        sname, svalue = syms[sidx][0], syms[sidx][1]
        is_memcpy, is_memset = sname == 'memcpy', sname == 'memset'
        p = base + r_offset
        word = lambda: struct.unpack('>I', out[r_offset:r_offset + 4])[0]
        if rtype in (1, 4, 6):
            v = DEADFEED if (is_memcpy or is_memset) else (base + svalue + r_addend) & 0xFFFFFFFF
            if rtype == 1:
                out[r_offset:r_offset + 4] = struct.pack('>I', v)
            elif rtype == 4:
                out[r_offset:r_offset + 2] = struct.pack('>H', v & 0xFFFF)
            else:
                out[r_offset:r_offset + 2] = struct.pack('>H', ((v >> 16) + ((v >> 15) & 1)) & 0xFFFF)
        elif rtype == 10:
            if is_memcpy: d = VENEER_MEMCPY - p
            elif is_memset: d = VENEER_MEMSET - p
            else: d = svalue + r_addend - r_offset
            assert -0x2000000 <= d < 0x2000000, 'bl out of range at 0x%08X' % p
            out[r_offset:r_offset + 4] = struct.pack('>I', (word() & ~0x03FFFFFC | (d & 0x03FFFFFC)) & 0xFFFFFFFF)
        elif rtype == 26:
            v = DEADFEED if (is_memcpy or is_memset) else (svalue + r_addend - r_offset) & 0xFFFFFFFF
            out[r_offset:r_offset + 4] = struct.pack('>I', v)
        # every other type is ignored by the game's switch
    return bytes(out)

def bl(frm, to):
    d = to - frm
    assert -0x2000000 <= d < 0x2000000, (hex(frm), hex(to))
    return struct.pack('>I', 0x48000001 | (d & 0x03FFFFFC))

def main():
    files, dol_in, dol_out, map_out, inc_out = sys.argv[1:6]
    levels = None
    if '--levels' in sys.argv:
        levels = {s.upper() for s in sys.argv[sys.argv.index('--levels') + 1].split(',')}
    modules, used = {}, set()
    for dirpath, _, names in os.walk(files):
        for n in sorted(names):
            if not n.lower().endswith('.ghr'): continue
            data = open(os.path.join(dirpath, n), 'rb').read()
            level = os.path.splitext(n)[0].upper()
            i = data.find(b'\x7fELF\x01\x02\x01')
            while i >= 0:
                text, rela, syms = parse(data, i)
                key = fnv1a64(rela, fnv1a64(text))
                if key in modules and modules[key][0] != text:
                    sys.exit('key collision %016x' % key)
                modules[key] = (text, rela, syms)
                if levels is None or level in levels: used.add(key)
                i = data.find(b'\x7fELF\x01\x02\x01', i + 4)
    # Stable placement: by key, 32-byte aligned (the loader warns below 16-byte alignment).
    addr, placed = FIRST_MODULE, []
    for key in sorted(modules):
        placed.append((key, addr)); addr = (addr + len(modules[key][0]) + 31) & ~31
    end = addr
    assert end <= OVERLAY_BASE + OVERLAY_SIZE, 'overlay full: 0x%X' % end
    image = bytearray(end - OVERLAY_BASE)
    image[0:4] = struct.pack('>I', 0x4E800020)  # LogicAlloc placeholder (blr); the runtime overrides it natively
    image[8:12] = struct.pack('>I', 0x4E800020)  # LogicFree placeholder (blr)
    image[0x10:0x14] = struct.pack('>I', 0x48000000 | ((MEMCPY - VENEER_MEMCPY) & 0x03FFFFFC))  # b memcpy
    image[0x18:0x1C] = struct.pack('>I', 0x48000000 | ((MEMSET - VENEER_MEMSET) & 0x03FFFFFC))  # b memset
    map_lines = ['%08X LogicAlloc_Scooby' % STUB_ALLOC, '%08X LogicFree_Scooby' % STUB_FREE,
                 '%08X LogicMemcpyVeneer' % VENEER_MEMCPY, '%08X LogicMemsetVeneer' % VENEER_MEMSET]
    for key, base in placed:
        text, rela, syms = modules[key]
        image[base - OVERLAY_BASE:base - OVERLAY_BASE + len(text)] = relocate(text, rela, syms, base)
        if key in used:
            seen = set()
            for name, value, info, shndx in syms:
                if info & 0xF == 2 and shndx != 0 and value not in seen:  # STT_FUNC in .text
                    seen.add(value)
                    clean = ''.join(c if c.isalnum() or c == '_' else '_' for c in name)
                    map_lines.append('%08X L%016x_%s' % (base + value, key, clean))
    # main.dol + text section 2 + the two patched call sites.
    dol = bytearray(open(dol_in, 'rb').read())
    u = lambda o: struct.unpack('>I', dol[o:o + 4])[0]
    assert u(2 * 4) == 0 and u(0x90 + 2 * 4) == 0, 'text section 2 already used'
    def file_offset(a):
        for k in range(18):
            s, sz = u(0x48 + k * 4), u(0x90 + k * 4)
            if sz and s <= a < s + sz: return u(k * 4) + a - s
        raise ValueError(hex(a))
    assert dol[file_offset(CALL_ALLOC):file_offset(CALL_ALLOC) + 4] == bl(CALL_ALLOC, MMMALLOC)
    assert dol[file_offset(CALL_FREE):file_offset(CALL_FREE) + 4] == bl(CALL_FREE, MMFREE)
    dol[file_offset(CALL_ALLOC):file_offset(CALL_ALLOC) + 4] = bl(CALL_ALLOC, STUB_ALLOC)
    dol[file_offset(CALL_FREE):file_offset(CALL_FREE) + 4] = bl(CALL_FREE, STUB_FREE)
    dol += b'\0' * (-len(dol) % 32)
    off = len(dol)
    dol += image
    dol[2 * 4:2 * 4 + 4] = struct.pack('>I', off)
    dol[0x48 + 2 * 4:0x48 + 2 * 4 + 4] = struct.pack('>I', OVERLAY_BASE)
    dol[0x90 + 2 * 4:0x90 + 2 * 4 + 4] = struct.pack('>I', len(image))
    open(dol_out, 'wb').write(dol)
    open(map_out, 'w').write('\n'.join(map_lines) + '\n')
    with open(inc_out, 'w') as f:
        f.write('// AUTO-GENERATED by projects/scooby/tools/build_logic_overlay.py: logic module key -> overlay address, .text size\n')
        for key, base in placed:
            f.write('{0x%016XULL, 0x%08Xu, 0x%Xu},\n' % (key, base, len(modules[key][0])))
    print('%d unique modules, overlay 0x%08X-0x%08X (%d KB), %d translated (%d MAP entries)'
          % (len(modules), OVERLAY_BASE, end, (end - OVERLAY_BASE) // 1024, len(used), len(map_lines) - 4))

if __name__ == '__main__':
    main()

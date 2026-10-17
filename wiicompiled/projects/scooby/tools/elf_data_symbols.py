"""Dump every object/notype symbol with an address from the release ELF. Usage: elf_data_symbols.py <elf> <out.txt>
Lines: <addr> <size> <bind L|G> <name>. Local statics keep their plain names in CodeWarrior ELFs, so duplicates are possible."""
import struct, sys
b = open(sys.argv[1], 'rb').read()
shoff, = struct.unpack('>I', b[0x20:0x24]); shnum, = struct.unpack('>H', b[0x30:0x32])
secs = [struct.unpack('>10I', b[shoff+i*40:shoff+i*40+40]) for i in range(shnum)]
cs = lambda o: b[o:b.index(b'\0', o)].decode('latin1')
rows = []
for s in secs:
    if s[1] != 2: continue
    st = secs[s[6]][4]
    for i in range(s[5] // 16):
        n, v, sz, info, _o, shn = struct.unpack('>IIIBBH', b[s[4]+i*16:s[4]+i*16+16])
        if (info & 15) in (0, 1) and v >= 0x80000000 and shn: rows.append((v, sz, 'G' if info >> 4 else 'L', cs(st + n)))
open(sys.argv[2], 'w').write(''.join('%08X %x %s %s\n' % r for r in sorted(rows)))
print(len(rows), 'data symbols')

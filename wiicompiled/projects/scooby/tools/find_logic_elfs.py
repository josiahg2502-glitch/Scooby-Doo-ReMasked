"""Find the PowerPC relocatable ELF objects (EFLogicCode logic modules) embedded in the disc files.
Usage: find_logic_elfs.py <Disc/DATA/files> -> prints one line per object: file, offset, .text size, symbols, relocs."""
import os, struct, sys
from collections import Counter
root = sys.argv[1]
found, kinds = [], Counter()
for dirpath, _, names in os.walk(root):
    for n in names:
        p = os.path.join(dirpath, n)
        data = open(p, 'rb').read()
        i = data.find(b'\x7fELF\x01\x02\x01')
        while i >= 0:
            h = data[i:i + 52]
            if len(h) == 52:
                etype, mach = struct.unpack('>HH', h[16:20])
                shoff, = struct.unpack('>I', h[32:36]); shnum, shstr = struct.unpack('>HH', h[48:52])
                if etype == 1 and mach == 20 and i + shoff + shnum * 40 <= len(data):
                    secs = [struct.unpack('>10I', data[i + shoff + k * 40:i + shoff + k * 40 + 40]) for k in range(shnum)]
                    so = i + secs[shstr][4]
                    nm = lambda o: data[so + o:data.index(b'\0', so + o)].decode('latin1')
                    info = {nm(s[0]): s for s in secs}
                    text = info.get('.text'); rela = info.get('.rela.text'); sym = info.get('.symtab')
                    for s in secs:
                        kinds[nm(s[0])] += 1
                    found.append((os.path.relpath(p, root), i, text[5] if text else 0,
                                  sym[5] // 16 if sym else 0, rela[5] // 12 if rela else 0))
            i = data.find(b'\x7fELF\x01\x02\x01', i + 4)
for f in found: print('%-60s off=0x%06X text=%6d syms=%4d relocs=%4d' % f)
print(len(found), 'objects;', 'total text', sum(f[2] for f in found), 'bytes; sections seen:', dict(kinds))
print('unique files:', len({f[0] for f in found}))

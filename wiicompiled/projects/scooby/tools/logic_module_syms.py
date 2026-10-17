"""Print one logic module's symbols (and relocations against them) by module key.
Usage: logic_module_syms.py <Disc/DATA/files> <key hex> [offset hex]"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
import build_logic_overlay as b
files, want = sys.argv[1], int(sys.argv[2], 16)
near = int(sys.argv[3], 16) if len(sys.argv) > 3 else None
for dirpath, _, names in os.walk(files):
    for n in names:
        if not n.lower().endswith('.ghr'): continue
        data = open(os.path.join(dirpath, n), 'rb').read()
        i = data.find(b'\x7fELF\x01\x02\x01')
        while i >= 0:
            text, rela, syms = b.parse(data, i)
            if b.fnv1a64(rela, b.fnv1a64(text)) == want:
                print(n, 'text', hex(len(text)))
                for k, (name, value, info, shndx) in enumerate(syms):
                    if near is None or abs(value - near) < 0x200:
                        print('  sym %3d %-40s value=0x%05X type=%d bind=%d shndx=%d' % (k, name, value, info & 15, info >> 4, shndx))
                sys.exit(0)
            i = data.find(b'\x7fELF\x01\x02\x01', i + 4)

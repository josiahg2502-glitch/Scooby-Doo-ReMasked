"""Count unique EFLogicCode modules (keyed by .text + .rela.text + .symtab bytes) and their total .text size.
Usage: logic_unique.py <Disc/DATA/files>"""
import hashlib, os, struct, sys
root = sys.argv[1]
uniq, per_level = {}, {}
for dirpath, _, names in os.walk(root):
    for n in names:
        if not n.endswith('.ghr'): continue
        p = os.path.join(dirpath, n); data = open(p, 'rb').read()
        i = data.find(b'\x7fELF\x01\x02\x01')
        while i >= 0:
            shoff, = struct.unpack('>I', data[i + 32:i + 36]); shnum, shstr = struct.unpack('>HH', data[i + 48:i + 52])
            secs = [struct.unpack('>10I', data[i + shoff + k * 40:i + shoff + k * 40 + 40]) for k in range(shnum)]
            so = i + secs[shstr][4]
            nm = lambda o: data[so + o:data.index(b'\0', so + o)]
            body = b''.join(data[i + s[4]:i + s[4] + s[5]] for s in secs if nm(s[0]) in (b'.text', b'.rela.text', b'.symtab'))
            text = next(s[5] for s in secs if nm(s[0]) == b'.text')
            uniq[hashlib.sha1(body).hexdigest()] = text
            per_level.setdefault(n, set()).add(hashlib.sha1(body).hexdigest())
            i = data.find(b'\x7fELF\x01\x02\x01', i + 4)
print(len(uniq), 'unique modules,', sum(uniq.values()) // 1024, 'KB of unique .text')
print('largest level set:', max(len(v) for v in per_level.values()), 'modules,',
      max(sum(uniq[h] for h in v) for v in per_level.values()) // 1024, 'KB')

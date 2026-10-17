"""Make a Windows .ico from the disc's 32x32 memory-card icon (uncompressed 32-bit TGA).
Usage: tga_to_ico.py <icon.tga> <out.ico>. Writes 32, 64, 128 and 256 px PNG entries (nearest-neighbour, pixel art)."""
import struct, sys, zlib
b = open(sys.argv[1], 'rb').read()
assert b[2] == 2 and b[16] == 32, 'expected an uncompressed 32-bit TGA'
w, h = struct.unpack('<HH', b[12:16]); top_down = b[17] & 0x20
px = b[18 + b[0]:]
rows = [px[y*w*4:(y+1)*w*4] for y in range(h)]
if not top_down: rows.reverse()
rgba = [[r[x*4+2:x*4+3] + r[x*4+1:x*4+2] + r[x*4:x*4+1] + r[x*4+3:x*4+4] for x in range(w)] for r in rows]  # BGRA -> RGBA per pixel
def png(scale):
    W, H = w * scale, h * scale
    raw = b''.join(b'\0' + b''.join(row[x // scale] for x in range(W)) for row in rgba for _ in range(scale))
    chunk = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
imgs = [(s * w, png(s)) for s in (1, 2, 4, 8)]
out = struct.pack('<HHH', 0, 1, len(imgs)); off = 6 + 16 * len(imgs)
for size, data in imgs:
    out += struct.pack('<BBBBHHII', size % 256, size % 256, 0, 0, 1, 32, len(data), off); off += len(data)
open(sys.argv[2], 'wb').write(out + b''.join(d for _, d in imgs))
print('wrote', sys.argv[2], [s for s, _ in imgs])

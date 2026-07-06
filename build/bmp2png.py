"""bmp2png.py - convert an 8-bit indexed BMP to a truecolor PNG (verification helper)."""
import struct, zlib, sys

def convert(src, dst):
    d = open(src, 'rb').read()
    pixoff = struct.unpack_from('<I', d, 10)[0]
    w = struct.unpack_from('<i', d, 18)[0]
    h = struct.unpack_from('<i', d, 22)[0]
    pal = d[54:54 + 256 * 4]
    rows = []
    for y in range(h - 1, -1, -1):
        o = pixoff + y * w
        raw = bytearray([0])
        for x in range(w):
            idx = d[o + x]
            b, g, r, _ = pal[idx * 4:idx * 4 + 4]
            raw += bytes((r, g, b))
        rows.append(bytes(raw))
    img = b''.join(rows)

    def chunk(t, data):
        c = t + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)

    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(img, 9))
    png += chunk(b'IEND', b'')
    open(dst, 'wb').write(png)
    print('wrote', dst, w, h)

if __name__ == '__main__':
    convert(sys.argv[1], sys.argv[2])

"""mkfloppy.py - build a 1.44 MB FAT12 floppy image containing the game.

Usage:  python build/mkfloppy.py AYRIEN.IMG AYRIEN.EXE dist/README.TXT ...
Produces a standard 1,474,560-byte FAT12 image mountable with
`imgmount a IMG -t floppy` in DOSBox / on real hardware.
"""
import sys, os, struct

SECT = 512
TOTAL_SECT = 2880          # 1.44 MB
RESERVED = 1
NFATS = 2
SPF = 9                    # sectors per FAT
ROOT_ENTS = 224
ROOT_SECT = ROOT_ENTS * 32 // SECT           # 14
DATA_START = RESERVED + NFATS * SPF + ROOT_SECT   # sector 33
BYTES_PER_CLUS = SECT      # 1 sector/cluster

def boot_sector():
    b = bytearray(SECT)
    b[0:3] = b'\xEB\x3C\x90'
    b[3:11] = b'MSDOS5.0'
    struct.pack_into('<H', b, 11, SECT)        # bytes/sector
    b[13] = 1                                   # sectors/cluster
    struct.pack_into('<H', b, 14, RESERVED)     # reserved sectors
    b[16] = NFATS
    struct.pack_into('<H', b, 17, ROOT_ENTS)
    struct.pack_into('<H', b, 19, TOTAL_SECT)
    b[21] = 0xF0                                # media descriptor
    struct.pack_into('<H', b, 22, SPF)
    struct.pack_into('<H', b, 24, 18)           # sectors/track
    struct.pack_into('<H', b, 26, 2)            # heads
    b[38] = 0x29                                # extended boot sig
    struct.pack_into('<I', b, 39, 0x12345678)   # volume serial
    b[43:54] = b'AYRIEN     '                    # volume label (11)
    b[54:62] = b'FAT12   '
    b[510] = 0x55; b[511] = 0xAA
    return b

def name83(fn):
    fn = os.path.basename(fn).upper()
    stem, _, ext = fn.partition('.')
    return (stem[:8].ljust(8) + ext[:3].ljust(3)).encode('ascii')

def build(out, files):
    if not files:
        raise SystemExit("no input files")
    if len(files) > ROOT_ENTS:
        raise SystemExit("too many root-directory entries")
    names = [name83(path) for path in files]
    if len(set(names)) != len(names):
        raise SystemExit("duplicate FAT 8.3 filename")
    missing = [path for path in files if not os.path.isfile(path)]
    if missing:
        raise SystemExit("missing input: %s" % missing[0])
    required_clusters = sum(max(1, (os.path.getsize(path) + BYTES_PER_CLUS - 1) // BYTES_PER_CLUS)
                            for path in files)
    available_clusters = TOTAL_SECT - DATA_START
    if required_clusters > available_clusters:
        raise SystemExit("floppy capacity exceeded: %d > %d clusters" %
                         (required_clusters, available_clusters))
    img = bytearray(TOTAL_SECT * SECT)
    img[0:SECT] = boot_sector()

    fat = [0] * (TOTAL_SECT)      # cluster -> next; index by cluster number
    fat[0] = 0xFF0
    fat[1] = 0xFFF
    root = bytearray()
    next_clus = 2

    for path in files:
        data = open(path, 'rb').read()
        nclus = max(1, (len(data) + BYTES_PER_CLUS - 1) // BYTES_PER_CLUS)
        first = next_clus
        for i in range(nclus):
            c = first + i
            fat[c] = 0xFFF if i == nclus - 1 else (c + 1)
            off = (DATA_START + (c - 2)) * SECT
            chunk = data[i * BYTES_PER_CLUS:(i + 1) * BYTES_PER_CLUS]
            img[off:off + len(chunk)] = chunk
        next_clus += nclus

        ent = bytearray(32)
        ent[0:11] = name83(path)
        ent[11] = 0x20                                  # archive
        struct.pack_into('<H', ent, 22, 0)              # time
        struct.pack_into('<H', ent, 24, 0x5721)         # date (2023-09-01-ish)
        struct.pack_into('<H', ent, 26, first)          # first cluster
        struct.pack_into('<I', ent, 28, len(data))      # size
        root += ent

    # encode FAT12 (two 12-bit entries per 3 bytes)
    fatbytes = bytearray(SPF * SECT)
    for i in range(0, TOTAL_SECT - 1, 2):
        v0 = fat[i] & 0xFFF
        v1 = fat[i + 1] & 0xFFF
        j = i * 3 // 2
        if j + 2 >= len(fatbytes): break
        fatbytes[j]     = v0 & 0xFF
        fatbytes[j + 1] = ((v0 >> 8) & 0x0F) | ((v1 & 0x0F) << 4)
        fatbytes[j + 2] = (v1 >> 4) & 0xFF

    for n in range(NFATS):
        off = (RESERVED + n * SPF) * SECT
        img[off:off + SPF * SECT] = fatbytes

    root_off = (RESERVED + NFATS * SPF) * SECT
    img[root_off:root_off + len(root)] = root

    open(out, 'wb').write(img)
    print("wrote %s : %d bytes, %d file(s), %d clusters used"
          % (out, len(img), len(files), next_clus - 2))

if __name__ == '__main__':
    build(sys.argv[1], sys.argv[2:])

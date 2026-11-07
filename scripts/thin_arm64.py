import struct, sys
src, dst, want = sys.argv[1], sys.argv[2], 0x0100000C  # CPU_TYPE_ARM64
d = open(src, "rb").read()
magic, n = struct.unpack_from(">II", d, 0)
assert magic == 0xCAFEBABE, hex(magic)
for i in range(n):
    cpu, sub, off, size, align = struct.unpack_from(">iiIII", d, 8 + 20 * i)
    if cpu == want:
        open(dst, "wb").write(d[off:off + size]); print("arm64 slice", hex(off), size); break
else: sys.exit("no arm64")

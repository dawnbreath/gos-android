# Packs the streaming assets (originals, with converted bundles replacing
# theirs) into stored zip parts below a size limit, each extractable on its own
# into the phone's shared storage root: GoSRecomp/StreamingAssets/...
#
# usage: python3 -I make_data_parts.py <Data/StreamingAssets> <converted StreamingAssets> <out dir> [limit MiB]
import os, sys, zipfile

src, conv, out = sys.argv[1:4]
limit = int(sys.argv[4]) * 2**20 if len(sys.argv) > 4 else 470 * 2**20
files = []
for root, _, names in os.walk(src):
    for n in names:
        p = os.path.join(root, n)
        rel = os.path.relpath(p, src)
        c = os.path.join(conv, rel)
        files.append((rel, c if os.path.exists(c) else p))
files.sort()
os.makedirs(out, exist_ok=True)
part, size, z = 0, 0, None
for rel, path in files:
    n = os.path.getsize(path)
    if z is None or size + n > limit:
        if z:
            z.close()
        part += 1
        size = 0
        z = zipfile.ZipFile(os.path.join(out, f"GoSRecomp-data-{part:02d}.zip"), "w", zipfile.ZIP_STORED)
    z.write(path, "GoSRecomp/StreamingAssets/" + rel)
    size += n
z.close()
print(f"{len(files)} files in {part} parts")

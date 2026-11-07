# Validates every GLES3 subprogram of converted asset files with glslangValidator.
#
# usage: python3 -I validate_gles.py <upstream tools/shaders> <work dir> <asset files...>
import collections, hashlib, os, subprocess, sys
import lz4.block

sys.path.insert(0, sys.argv[1])
import UnityPy
from convert_shaders import read_entries, read_subprogram  # upstream

work = sys.argv[2]
os.makedirs(work, exist_ok=True)
stats, errors, examples, done = collections.Counter(), collections.Counter(), {}, set()
for path in sys.argv[3:]:
    env = UnityPy.load(path)
    for obj in env.objects:
        if obj.type.name != "Shader":
            continue
        t = obj.read_typetree()
        if list(t["platforms"]) != [9]:
            continue
        raw = lz4.block.decompress(bytes(t["compressedBlob"])[:t["compressedLengths"][0][0]],
                                   uncompressed_size=t["decompressedLengths"][0][0])
        for o, n, _ in read_entries(raw):
            src = read_subprogram(raw[o:o + n])["code"].decode()
            h = hashlib.sha1(src.encode()).hexdigest()[:12]
            if h in done:
                continue
            done.add(h)
            ok = True
            for stage, ext in (("VERTEX", "vert"), ("FRAGMENT", "frag")):
                a = src.index(f"#ifdef {stage}\n") + len(f"#ifdef {stage}\n")
                b = src.index("\n#endif\n#ifdef FRAGMENT") if stage == "VERTEX" else src.rindex("\n#endif")
                p = os.path.join(work, f"{h}.{ext}")
                with open(p, "w") as f:
                    f.write(src[a:b].replace("UNITY_SUPPORTS_UNIFORM_LOCATION 1", "UNITY_SUPPORTS_UNIFORM_LOCATION 0"))
                r = subprocess.run(["glslangValidator", p], capture_output=True, text=True)
                if r.returncode:
                    ok = False
                    line = next((l for l in r.stdout.splitlines() if l.startswith("ERROR:")), "?")
                    key = f"{t['m_ParsedForm']['m_Name']}: {line.split(':', 3)[-1].strip()[:80]}"
                    errors[key] += 1
                    examples.setdefault(key, p)
            # link check: vertex outputs vs fragment inputs
            if ok:
                r = subprocess.run(["glslangValidator", "-l", os.path.join(work, f"{h}.vert"), os.path.join(work, f"{h}.frag")],
                                   capture_output=True, text=True)
                if r.returncode:
                    ok = False
                    line = next((l for l in r.stdout.splitlines() if l.startswith("ERROR:")), "?")
                    key = f"link {t['m_ParsedForm']['m_Name']}: {line[:90]}"
                    errors[key] += 1
                    examples.setdefault(key, p)
            stats["valid" if ok else "invalid"] += 1
print(dict(stats))
for k, v in errors.most_common(30):
    print(f"{v:5d}  {k}   e.g. {examples[k]}")

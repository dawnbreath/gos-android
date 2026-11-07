# Translates every (vertex, fragment) pair of the first variants of each pass
# in the given asset files and validates the GLSL ES with glslangValidator.
#
# usage: python3 -I test_msl2glsl.py <upstream tools/shaders> <work dir> <asset files...>
import collections, os, subprocess, sys
import lz4.block

sys.path.insert(0, sys.argv[1])
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import UnityPy
from convert_shaders import read_entries, read_subprogram, msl_of  # upstream
from msl2hlsl import Unsupported
import msl2glsl

work = sys.argv[2]
os.makedirs(work, exist_ok=True)
stats = collections.Counter()
errors = collections.Counter()
examples = {}


def validate(src, tag):
    ok = True
    for stage, ext in (("VERTEX", "vert"), ("FRAGMENT", "frag")):
        a = src.index(f"#ifdef {stage}\n") + len(f"#ifdef {stage}\n")
        b = src.index("\n#endif\n#ifdef FRAGMENT") if stage == "VERTEX" else src.rindex("\n#endif")
        path = os.path.join(work, f"{tag}.{ext}")
        with open(path, "w") as f:
            f.write(src[a:b].replace("#define UNITY_SUPPORTS_UNIFORM_LOCATION 1", "#define UNITY_SUPPORTS_UNIFORM_LOCATION 0"))
        r = subprocess.run(["glslangValidator", path], capture_output=True, text=True)
        if r.returncode != 0:
            ok = False
            for line in r.stdout.splitlines():
                if line.startswith("ERROR:") and "compilation terminated" not in line:
                    key = line.split(":", 3)[-1].strip()[:90]
                    errors[key] += 1
                    examples.setdefault(key, path)
                    break
    return ok


n = 0
for path in sys.argv[3:]:
    env = UnityPy.load(path)
    for obj in env.objects:
        if obj.type.name != "Shader":
            continue
        d = obj.read()
        if list(d.platforms) != [14]:
            continue
        blob = bytes(d.compressedBlob)
        off, cl, dl = d.offsets[0][0], d.compressedLengths[0][0], d.decompressedLengths[0][0]
        raw = lz4.block.decompress(blob[off:off + cl], uncompressed_size=dl)
        subs = [read_subprogram(raw[o:o + k]) for o, k, _ in read_entries(raw)]
        for ss in d.m_ParsedForm.m_SubShaders:
            for p in ss.m_Passes:
                vs = [sp.m_BlobIndex for sp in (p.progVertex.m_SubPrograms or [])]
                fs = [sp.m_BlobIndex for sp in (p.progFragment.m_SubPrograms or [])]
                for vi, fi in list(zip(vs, fs))[:3]:
                    n += 1
                    try:
                        src, _, _ = msl2glsl.translate_pair(msl_of(subs[vi]), msl_of(subs[fi]))
                    except Unsupported as e:
                        stats["unsupported"] += 1
                        errors["UNSUPPORTED " + str(e)[:80]] += 1
                        continue
                    stats["valid" if validate(src, f"p{n:05d}") else "invalid"] += 1

print(dict(stats))
for k, v in errors.most_common(40):
    print(f"{v:5d}  {k}   e.g. {examples.get(k, '')}")

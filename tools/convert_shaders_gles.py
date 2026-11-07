# Rewrites the Metal-only Shader objects of the game's asset files (and asset
# bundles) as OpenGL ES 3 shaders, for Unity's Android player.
#
# Metal keeps vertex and fragment programs apart (progVertex/progFragment);
# Unity's GL backend wants one program per variant holding both stages, listed
# under progVertex only. For every pass, each fragment variant is paired with
# its best-matching vertex variant (and the other way round); each pair becomes
# one GLES3 subprogram whose keywords are the union of both.
#
# All parameters go into the blob as one "$Globals" constant buffer of plain
# uniforms (the parsed form's common parameters are cleared, as in Unity's own
# GLES3 shaders), with offsets of this converter's choosing.
#
# usage: python3 -I convert_shaders_gles.py <upstream tools/shaders> <out dir> <asset files...>
import collections, copy, os, re, sys
import lz4.block

sys.path.insert(0, sys.argv[1])
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import UnityPy
from convert_shaders import read_entries, read_subprogram, write_subprogram, write_entries, msl_of  # upstream
from msl2hlsl import Unsupported, split_type
import msl2glsl

PLATFORM_METAL, PLATFORM_GLES3 = 14, 9
METAL_VS, METAL_FS = 23, 24
GLES3 = 4
BLOB_DIM = {"texture2d": 4, "depth2d": 4, "texture3d": 6, "texturecube": 8, "depthcube": 8,
            "texture2d_array": 10, "depth2d_array": 10}

STUB = """#ifdef VERTEX
#version 300 es

void main()
{
    gl_Position = vec4(0.0, 0.0, 0.0, 0.0);
}
#endif
#ifdef FRAGMENT
#version 300 es

precision highp float;
layout(location = 0) out highp vec4 SV_Target0;
void main()
{
    discard;
}
#endif
"""


def unity_params(uniforms):
    """$Globals parameters for the GLSL uniforms, with fresh offsets."""
    params, off = [], 0
    for name, mtype, arr in uniforms:
        base, n = split_type(mtype)
        ptype = 0 if base == "float" else 1
        m = re.fullmatch(r"hlslcc_mtx(\d)x(\d)(\w+)", name)
        if m and n == 4 and arr:
            rows = int(m.group(1))
            count = arr // rows
            params.append({"name": m.group(3), "type": ptype, "rows": rows, "cols": int(m.group(2)), "matrix": 1,
                           "array": count if count > 1 else 0, "offset": off})
            off += 16 * arr
            continue
        params.append({"name": name, "type": ptype, "rows": 1, "cols": n, "matrix": 0, "array": arr, "offset": off})
        off += 16 * (arr or 1)
    return params, off


def gles_subprogram(vs_sub, fs_sub):
    try:
        src, uniforms, textures = msl2glsl.translate_pair(msl_of(vs_sub), msl_of(fs_sub))
        stub = False
    except Unsupported as e:
        src, uniforms, textures, stub = STUB, [], [], str(e)
    params, size = unity_params(uniforms)
    kws = list(dict.fromkeys(vs_sub["keywords"] + fs_sub["keywords"]))
    lkws = list(dict.fromkeys(vs_sub["local_keywords"] + fs_sub["local_keywords"]))
    o = {
        "version": vs_sub["version"], "type": GLES3, "stats": [0, 0, 0, 0],
        "keywords": kws, "local_keywords": lkws,
        "code": src.encode("utf-8"),
        "source_map": vs_sub["source_map"], "channels": [],
        "cbuffers": [{"name": "", "size": 0, "params": [], "struct_params": 0},
                     {"name": "$Globals", "size": size, "params": params, "struct_params": 0}],
        "bindings": [{"name": name, "kind": 0, "index": unit, "extra": unit, "dim": BLOB_DIM.get(kind, 4)}
                     for name, unit, kind, elem in textures],
    }
    return o, stub


def keyset(e):
    return frozenset(("g", k) for k in (e["m_GlobalKeywordIndices"] or [])) | \
           frozenset(("l", k) for k in (e["m_LocalKeywordIndices"] or []))


def best(entry, candidates):
    """Candidate whose keywords match `entry` best: most shared, then fewest extra."""
    k = keyset(entry)
    return max(candidates, key=lambda c: (len(k & keyset(c)), -len(keyset(c) - k)))


def pair_entries(vs_entries, fs_entries):
    """[(vs entry, fs entry)] covering every variant of either stage."""
    pairs, seen = [], set()
    by_tier = collections.defaultdict(lambda: ([], []))
    for e in vs_entries:
        by_tier[e["m_ShaderHardwareTier"]][0].append(e)
    for e in fs_entries:
        by_tier[e["m_ShaderHardwareTier"]][1].append(e)
    for tier, (vs, fs) in sorted(by_tier.items()):
        if not vs or not fs:
            continue
        cand = [(best(f, vs), f) for f in fs] + [(v, best(v, fs)) for v in vs]
        for v, f in cand:
            key = (tier, keyset(v) | keyset(f))
            if key in seen:
                continue
            seen.add(key)
            pairs.append((v, f))
    return pairs


def empty_params(template):
    p = copy.deepcopy(template)
    for k in p:
        if isinstance(p[k], list):
            p[k] = []
    return p


def convert_shader(t, log):
    raw = lz4.block.decompress(bytes(t["compressedBlob"])[t["offsets"][0][0]:][:t["compressedLengths"][0][0]],
                               uncompressed_size=t["decompressedLengths"][0][0])
    subs = [read_subprogram(raw[o:o + n]) for o, n, _ in read_entries(raw)]
    name = t["m_ParsedForm"]["m_Name"]
    blobs, blob_index = [], {}
    for ss in t["m_ParsedForm"]["m_SubShaders"]:
        for p in ss["m_Passes"]:
            vs_e = p["progVertex"]["m_SubPrograms"] or []
            fs_e = p["progFragment"]["m_SubPrograms"] or []
            entries = []
            if vs_e and fs_e:
                for v, f in pair_entries(vs_e, fs_e):
                    key = (v["m_BlobIndex"], f["m_BlobIndex"])
                    if key not in blob_index:
                        o, stub = gles_subprogram(subs[v["m_BlobIndex"]], subs[f["m_BlobIndex"]])
                        if stub:
                            log[f"stub: {stub}"] += 1
                        log["programs"] += 1
                        blob_index[key] = len(blobs)
                        blobs.append(write_subprogram(o))
                    e = copy.deepcopy(v)
                    e["m_BlobIndex"] = blob_index[key]
                    e["m_GpuProgramType"] = GLES3
                    e["m_GlobalKeywordIndices"] = sorted(set(v["m_GlobalKeywordIndices"] or []) | set(f["m_GlobalKeywordIndices"] or []))
                    e["m_LocalKeywordIndices"] = sorted(set(v["m_LocalKeywordIndices"] or []) | set(f["m_LocalKeywordIndices"] or []))
                    e["m_ShaderRequirements"] = v["m_ShaderRequirements"] | f["m_ShaderRequirements"]
                    e["m_Channels"] = {"m_Channels": [], "m_SourceMap": v["m_Channels"]["m_SourceMap"]}
                    e["m_Parameters"] = empty_params(v["m_Parameters"])
                    entries.append(e)
            elif vs_e or fs_e:
                log[f"{name}: pass with a single stage"] += 1
            p["progVertex"]["m_SubPrograms"] = entries
            p["progFragment"]["m_SubPrograms"] = []
            for prog in ("progVertex", "progFragment"):
                p[prog]["m_CommonParameters"] = empty_params(p[prog]["m_CommonParameters"])
            p["m_Platforms"] = [PLATFORM_GLES3]
    raw = write_entries(blobs)
    comp = lz4.block.compress(raw, store_size=False, mode="high_compression")
    t["platforms"] = [PLATFORM_GLES3]
    t["offsets"] = [[0]]
    t["compressedLengths"] = [[len(comp)]]
    t["decompressedLengths"] = [[len(raw)]]
    t["compressedBlob"] = comp
    return len(blobs)


def convert_file(path, out_dir, log):
    env = UnityPy.load(path)
    changed = 0
    for obj in env.objects:
        if obj.type.name != "Shader":
            continue
        t = obj.read_typetree()
        if list(t["platforms"]) != [PLATFORM_METAL]:
            continue
        if len(t["offsets"][0]) != 1:
            log["segmented blob"] += 1
            continue
        convert_shader(t, log)
        obj.save_typetree(t)
        changed += 1
    if not changed:
        return 0
    os.makedirs(out_dir, exist_ok=True)
    is_bundle = type(env.file).__name__ == "BundleFile"
    with open(os.path.join(out_dir, os.path.basename(path)), "wb") as f:
        f.write(env.file.save(packer="lz4") if is_bundle else env.file.save())
    return changed


if __name__ == "__main__":
    log = collections.Counter()
    out_dir = sys.argv[2]
    for path in sys.argv[3:]:
        n = convert_file(path, out_dir, log)
        if n:
            print(f"{os.path.basename(path)}: {n} shaders")
    for k, v in log.most_common(30):
        print(f"  {v:6d}  {k}")

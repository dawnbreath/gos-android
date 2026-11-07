# Builds ft_all.json (the translator's main table) from:
#   function_table.json  il2cpp-section functions (upstream tools/analysis/function_table.py)
#   text_class.json      __text functions classified as generated code (upstream classify_text.py)
# Sizes of __text functions come from LC_FUNCTION_STARTS (distance to the next start).
# Upstream BUILDING.md names both inputs but not this merge step.
#
# usage: python3 -I merge_ft_all.py <GameAssembly arm64> <function_table.json> <text_class.json> <out.json>
import sys, json, bisect
import lief

binary, table, text_class, out = sys.argv[1:5]
b = lief.parse(binary)
text = b.get_section("__text")
tva, tend = text.virtual_address, text.virtual_address + text.size
starts = sorted(a for a in b.function_starts.functions if tva <= a < tend)

ft = json.load(open(table))
have = {f["addr"] for f in ft["functions"]}
added = 0
for a in json.load(open(text_class))["generated"]:
    if a in have:
        continue
    k = bisect.bisect_right(starts, a)
    end = starts[k] if k < len(starts) else tend
    ft["functions"].append({"addr": a, "size": end - a, "name": f"text_{a:x}"})
    added += 1
ft["functions"].sort(key=lambda f: f["addr"])
json.dump(ft, open(out, "w"))
print(f"functions: {len(ft['functions'])} (+{added} from __text)")

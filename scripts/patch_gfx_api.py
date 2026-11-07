# Declares OpenGL ES 3 as the graphics API the data was built for (the macOS
# build lists only Metal). Android counterpart of upstream
# tools/shaders/patch_build_settings.py.
#
# usage: python3 -I patch_gfx_api.py <globalgamemanagers> <out>
import sys
import UnityPy

GFX_GLES3 = 11  # GraphicsDeviceType.OpenGLES3 (Metal is 16)

env = UnityPy.load(sys.argv[1])
for obj in env.objects:
    if obj.type.name == "BuildSettings":
        tree = obj.read_typetree()
        print("m_GraphicsAPIs", tree["m_GraphicsAPIs"], "->", [GFX_GLES3])
        tree["m_GraphicsAPIs"] = [GFX_GLES3]
        obj.save_typetree(tree)
with open(sys.argv[2], "wb") as f:
    f.write(env.file.save())

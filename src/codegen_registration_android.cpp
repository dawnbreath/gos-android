// Android counterpart of upstream recomp/winbuild/codegen_registration.cpp:
// connects the game's IL2CPP registration tables (from the macOS arm64 image,
// loaded as data) to the native libil2cpp.
//
// The image is looked up, in order, at:
//   $GOSR_GAME_IMAGE
//   <il2cpp data dir>/GameAssembly_arm64.dylib   (data dir as set by libunity)
//   /sdcard/Android/data/<package>/files/GameAssembly_arm64.dylib
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <sys/stat.h>

#include <android/log.h>

#include "macho_image.h"
#include "recomp_bridge.h"
#include "recomp_ctx.h"
#include "recomp_eh.h"
#include "il2cpp-config.h"
#include "utils/Runtime.h"

struct Il2CppCodeRegistration;
struct Il2CppMetadataRegistration;
struct Il2CppCodeGenOptions;

void il2cpp_codegen_register(const Il2CppCodeRegistration* const codeRegistration,
                             const Il2CppMetadataRegistration* const metadataRegistration,
                             const Il2CppCodeGenOptions* const codeGenOptions);

namespace recomp
{
    void RegisterRuntimeImports();
    uint64_t ResolveImageSymbol(const MachOImage& image, const std::string& symbol);
    void InstallDebugMenu(uint64_t imageBase);
}

namespace
{
    // Offsets in the macOS v1.4.2 arm64 GameAssembly (same as upstream).
    constexpr uint64_t kCodeRegistration = 0x3ad2790;
    constexpr uint64_t kMetadataRegistration = 0x3ad3308;
    constexpr uint64_t kCodeGenOptions = 0x352a250;

    recomp::MachOImage* g_image = nullptr;

    bool Exists(const std::string& p)
    {
        struct stat st;
        return !p.empty() && stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
    }

    std::string PackageName()
    {
        std::string name;
        if (FILE* f = fopen("/proc/self/cmdline", "rb"))
        {
            int ch;
            while ((ch = fgetc(f)) > 0 && ch != ':')
                name += char(ch);
            fclose(f);
        }
        return name;
    }

    std::vector<std::string> Candidates()
    {
        std::vector<std::string> out;
        if (const char* env = getenv("GOSR_GAME_IMAGE"))
            out.push_back(env);
        std::string data = il2cpp::utils::Runtime::GetDataDir();
        if (!data.empty())
        {
            out.push_back(data + "/GameAssembly_arm64.dylib");
            out.push_back(data + "/../GameAssembly_arm64.dylib");
        }
        std::string pkg = PackageName();
        if (!pkg.empty())
        {
            out.push_back("/sdcard/Android/data/" + pkg + "/files/GameAssembly_arm64.dylib");
            out.push_back("/storage/emulated/0/Android/data/" + pkg + "/files/GameAssembly_arm64.dylib");
        }
        return out;
    }

    void RegisterGameCodegen()
    {
        g_image = new recomp::MachOImage;
        std::string path;
        for (const std::string& p : Candidates())
        {
            __android_log_print(ANDROID_LOG_INFO, "GoSRecomp", "game image candidate: %s%s", p.c_str(), Exists(p) ? " (found)" : "");
            if (path.empty() && Exists(p))
                path = p;
        }
        if (path.empty() || !g_image->Load(path))
        {
            __android_log_print(ANDROID_LOG_FATAL, "GoSRecomp", "cannot load the game image (%s): %s",
                                path.empty() ? "not found" : path.c_str(), g_image->GetError().c_str());
            abort();
        }
        recomp::g_imageBase = g_image->Base();
        recomp::g_imageSize = g_image->Size();
        auto unbound = g_image->ApplyBindings([](const std::string& symbol) { return recomp::ResolveImageSymbol(*g_image, symbol); });
        __android_log_print(ANDROID_LOG_INFO, "GoSRecomp", "game image %s at %p, %zu bindings left unbound",
                            path.c_str(), reinterpret_cast<void*>(g_image->Base()), unbound.size());
        if (getenv("GOSR_VERBOSE"))
            for (const auto& b : unbound)
                fprintf(stderr, "  unbound %s at 0x%llx\n", b.symbol.c_str(), static_cast<unsigned long long>(b.vmAddress));

        recomp::InstallBridges();
        recomp::InstallDebugMenu(g_image->Base());
        recomp::RegisterRuntimeImports();
        recomp::RegisterExceptionImports();

        uint64_t base = g_image->Base();
        il2cpp_codegen_register(reinterpret_cast<const Il2CppCodeRegistration*>(base + kCodeRegistration),
                                reinterpret_cast<const Il2CppMetadataRegistration*>(base + kMetadataRegistration),
                                reinterpret_cast<const Il2CppCodeGenOptions*>(base + kCodeGenOptions));
    }
}

void (*g_CodegenRegistration)() = RegisterGameCodegen;

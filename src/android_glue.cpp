// Android replacements for the Windows-only parts of upstream recomp/winbuild:
//   - stderr goes to logcat (tag GoSRecomp), so the upstream diagnostics show up
//   - crash_report.cpp: the main-thread sampler is not ported yet
//   - mouse_keys.cpp / display.cpp: no icall wrapping (first boot: game defaults)
//   - controller_hooks.cpp: hooks call the original functions unchanged
//   - debug_menu.cpp: not ported yet
#include <cstdio>
#include <pthread.h>
#include <string>
#include <unistd.h>

#include <android/log.h>

#include "recomp_ctx.h"

namespace
{
    void* PumpStderr(void* arg)
    {
        int fd = static_cast<int>(reinterpret_cast<intptr_t>(arg));
        char buf[1024];
        std::string line;
        ssize_t n;
        while ((n = read(fd, buf, sizeof buf)) > 0)
            for (ssize_t i = 0; i < n; i++)
            {
                if (buf[i] == '\n' || line.size() > 900)
                {
                    __android_log_write(ANDROID_LOG_INFO, "GoSRecomp", line.c_str());
                    line.clear();
                }
                if (buf[i] != '\n')
                    line += buf[i];
            }
        return nullptr;
    }

    __attribute__((constructor)) void RedirectStderr()
    {
        int fds[2];
        if (pipe(fds) != 0)
            return;
        setvbuf(stderr, nullptr, _IONBF, 0);
        dup2(fds[1], STDERR_FILENO);
        pthread_t t;
        pthread_create(&t, nullptr, PumpStderr, reinterpret_cast<void*>(intptr_t(fds[0])));
        pthread_detach(t);
        __android_log_write(ANDROID_LOG_INFO, "GoSRecomp", "libil2cpp.so (GoSRecomp Android) loaded");
    }
}

namespace recomp
{
    // crash_report.cpp
    void (*g_sampleStateHook)() = nullptr;
    void StartMainThreadSampler() {}

    // mouse_keys.cpp / display.cpp
    void* WrapInputIcall(const char*, void* original) { return original; }
    void* WrapDisplayIcall(const char*, void* original) { return original; }
    bool MouseButtonsEnabled() { return false; }

    // debug_menu.cpp
    void InstallDebugMenu(uint64_t) {}

    // controller_hooks.cpp: pass-through for now
    void f_200fbe0_orig(Ctx& c);
    void f_200f60c_orig(Ctx& c);
    void f_1ecc8f0_orig(Ctx& c);
    void HookIsTrigger(Ctx& c) { f_200fbe0_orig(c); }
    void HookJoystickInput(Ctx& c) { f_200f60c_orig(c); }
    void HookRebindInputSystemKeyDown(Ctx& c) { f_1ecc8f0_orig(c); }
}

// Symbols a normal Unity Android build gets from its own link:
//  - __start_il2cpp/__stop_il2cpp bound the "il2cpp" section holding generated
//    code (os/Posix/Image.cpp). The recompiled code lives in .text; a marker
//    function makes the linker create the section and the two symbols.
//  - __data_start/__end__ bound the main program's data for Boehm GC
//    (gcconfig.h), which this build tells not to register
//    (GC_DONT_REGISTER_MAIN_STATIC_DATA); they only need to resolve.
extern "C"
{
    __attribute__((section("il2cpp"), used, noinline)) void gosr_il2cpp_section_marker() {}
    __attribute__((visibility("hidden"))) int __data_start[1];
    __attribute__((visibility("hidden"))) int __end__[1];
}

// Android stand-ins for the game's macOS plugins (upstream plugins/apple,
// plugins/systemlang), one shared library each, chosen with -DGOSR_PLUGIN_<NAME>.
// IL2CPP's P/Invoke loader finds them as lib<BundleName>.so in the APK.
//
// SPFXUnityPlugin is a stub for now: it answers like the game's own
// SPFXPluginNull (effects load instantly and draw nothing).
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#define GOSR_EXPORT extern "C" __attribute__((visibility("default")))

namespace
{
    // Strings returned to a P/Invoke "string" result are freed by the
    // marshaler (free() on POSIX).
    char* MarshalString(const char* s)
    {
        if (!s)
            return nullptr;
        size_t n = strlen(s) + 1;
        char* out = static_cast<char*>(malloc(n));
        if (out)
            memcpy(out, s, n);
        return out;
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

    // The app's private files directory.
    std::string FilesDir()
    {
        return "/data/data/" + PackageName() + "/files";
    }
}

#ifdef GOSR_PLUGIN_FPBundleWrapper
GOSR_EXPORT void DoRegisterAppWithFairPlay() {}
#endif

#ifdef GOSR_PLUGIN_GameCenterAccessPointBundleWrapper
GOSR_EXPORT void _showAccessPoint(int) {}
GOSR_EXPORT void _hideAccessPoint() {}
GOSR_EXPORT void _showHighlights(int) {}
GOSR_EXPORT void _getAccessPointFrameInScreenCoord(float* x, float* y, float* w, float* h)
{
    if (x) *x = 0; if (y) *y = 0; if (w) *w = 0; if (h) *h = 0;
}
GOSR_EXPORT void _getMainScreenBounds(float* x, float* y, float* w, float* h)
{
    if (x) *x = 0; if (y) *y = 0; if (w) *w = 0; if (h) *h = 0;
}
GOSR_EXPORT int _isPresentingGameCenter() { return 0; }
GOSR_EXPORT int _isVisible() { return 0; }
GOSR_EXPORT void _trigger() {}
#endif

#ifdef GOSR_PLUGIN_iCloudBundleWrapper
// CloudKit is offline ("no account"); the NSUserDefaults store is kept as
// files in <files>/userdefaults, like upstream does next to the game.
namespace
{
    constexpr int kAccountStatusNoAccount = 3;

    bool KeyPath(const char* name, std::string& out, bool create)
    {
        if (!name || !*name)
            return false;
        std::string dir = FilesDir();
        if (create)
            mkdir(dir.c_str(), 0700);
        dir += "/userdefaults";
        if (create)
            mkdir(dir.c_str(), 0700);
        std::string key;
        for (const char* p = name; *p && key.size() < 120; p++)
        {
            char ch = *p;
            bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.';
            key += ok ? ch : '_';
        }
        out = dir + "/" + key + ".bin";
        return true;
    }
}

GOSR_EXPORT void CrowdKitManager_Initialize() {}
GOSR_EXPORT void CrowdKitManager_Terminate() {}
GOSR_EXPORT int CrowdKitManager_IsAvailableCloud() { return 0; }
GOSR_EXPORT int CrowdKitManager_GetAccountStatus() { return kAccountStatusNoAccount; }
GOSR_EXPORT void CrowdKitManager_RequestUpdteAccountStatus() {}
GOSR_EXPORT int CrowdKitManager_IsUpdatedAccountStatus() { return 1; }
GOSR_EXPORT void CrowdKitManager_FetchAtUploadSlotSaveData(int) {}
GOSR_EXPORT void CrowdKitManager_UploadSlotSaveData(const char*, int) {}
GOSR_EXPORT int CrowdKitManager_DownloadSlotSaveData(int) { return 0; }
GOSR_EXPORT int CrowdKitManager_DeleteSlotSaveData(int) { return 0; }
GOSR_EXPORT void CrowdKitManager_FetchAtUploadSystemSaveData() {}
GOSR_EXPORT void CrowdKitManager_UploadSystemSaveData(const char*) {}
GOSR_EXPORT int CrowdKitManager_DownloadSystemSaveData() { return 0; }
GOSR_EXPORT int CrowdKitManager_DeleteSystemSaveData() { return 0; }
GOSR_EXPORT void CrowdKitManager_SetUploadSlotSaveDataCallback(void*) {}
GOSR_EXPORT void CrowdKitManager_SetFetchAtUploadSlotSaveDataCallback(void*) {}
GOSR_EXPORT void CrowdKitManager_SetDownloadSlotSaveDataCallback(void*) {}
GOSR_EXPORT void CrowdKitManager_SetUploadSystemSaveDataCallback(void*) {}
GOSR_EXPORT void CrowdKitManager_SetFetchAtUploadSystemSaveDataCallback(void*) {}
GOSR_EXPORT void CrowdKitManager_SetDownloadSystemSaveDataCallback(void*) {}
GOSR_EXPORT int CrowdKitManager_GetDownloadSlotSaveDataSlotIndex() { return -1; }

GOSR_EXPORT void CrowdKitManager_WriteToUserDefaultData(const char* name, const unsigned char* data, int size)
{
    std::string path;
    if (!KeyPath(name, path, true) || size < 0 || (size > 0 && !data))
        return;
    std::string temp = path + ".tmp";
    FILE* f = fopen(temp.c_str(), "wb");
    if (!f)
        return;
    bool ok = size == 0 || fwrite(data, 1, size_t(size), f) == size_t(size);
    ok = fflush(f) == 0 && ok;
    fsync(fileno(f));
    fclose(f);
    if (ok)
        rename(temp.c_str(), path.c_str());
    else
        unlink(temp.c_str());
}

GOSR_EXPORT int CrowdKitManager_ReadFromUserDefaultData(void** outData, int* outSize, const char* name)
{
    if (outData) *outData = nullptr;
    if (outSize) *outSize = 0;
    std::string path;
    if (!outData || !outSize || !KeyPath(name, path, false))
        return 0;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f)
        return 0;
    int result = 0;
    if (fseek(f, 0, SEEK_END) == 0)
    {
        long n = ftell(f);
        rewind(f);
        if (n >= 0 && n < (64 << 20))
        {
            void* buffer = malloc(n ? size_t(n) : 1);
            if (buffer && (n == 0 || fread(buffer, 1, size_t(n), f) == size_t(n)))
            {
                *outData = buffer;
                *outSize = int(n);
                result = 1;
            }
            else
                free(buffer);
        }
    }
    fclose(f);
    return result;
}

GOSR_EXPORT int CrowdKitManager_DeleteUserDefaultData(const char* name)
{
    std::string path;
    return KeyPath(name, path, false) && unlink(path.c_str()) == 0 ? 1 : 0;
}
#endif

#ifdef GOSR_PLUGIN_GCControllerBundleWrapper
// Button glyph names as the Xbox layout (upstream picks by controller family).
#define GOSR_BUTTON(name, symbol, alias) \
    GOSR_EXPORT char* _get##name##SymbolName() { return MarshalString(symbol); } \
    GOSR_EXPORT char* _get##name##Alias() { return MarshalString(alias); }
GOSR_BUTTON(LeftShoulderButton, "lb.rectangle.roundedbottom", "LB")
GOSR_BUTTON(RightShoulderButton, "rb.rectangle.roundedbottom", "RB")
GOSR_BUTTON(LeftTriggerButton, "lt.rectangle.roundedtop", "LT")
GOSR_BUTTON(RightTriggerButton, "rt.rectangle.roundedtop", "RT")
GOSR_BUTTON(MenuButton, "line.horizontal.3.circle", "Menu")
GOSR_BUTTON(OptionsButton, "rectangle.fill.on.rectangle.fill.circle", "View")
GOSR_BUTTON(HomeButton, "house.circle", "Home")
GOSR_BUTTON(AButton, "a.circle", "A")
GOSR_BUTTON(BButton, "b.circle", "B")
GOSR_BUTTON(XButton, "x.circle", "X")
GOSR_BUTTON(YButton, "y.circle", "Y")
GOSR_BUTTON(DPad, "dpad", "D-pad")
GOSR_BUTTON(DPadUpButton, "dpad.up.filled", "D-pad Up")
GOSR_BUTTON(DPadDownButton, "dpad.down.filled", "D-pad Down")
GOSR_BUTTON(DPadLeftButton, "dpad.left.filled", "D-pad Left")
GOSR_BUTTON(DPadRightButton, "dpad.right.filled", "D-pad Right")
GOSR_BUTTON(LeftThumbStick, "l.joystick", "Left Stick")
GOSR_BUTTON(RightThumbStick, "r.joystick", "Right Stick")
GOSR_BUTTON(LeftThumbStickButton, "l.joystick.press.down", "LS")
GOSR_BUTTON(RightThumbStickButton, "r.joystick.press.down", "RS")
#endif

#ifdef GOSR_PLUGIN_SymbolImageBundleWrapper
// SF Symbols renderer: no image (the game falls back to its text alias).
GOSR_EXPORT int GetSymbolImage(void** data, int, int, const char*)
{
    if (data)
        *data = nullptr;
    return 0;
}
#endif

#ifdef GOSR_PLUGIN_SystemLangBundle
#include <sys/system_properties.h>
// BCP 47 language tag of the device (DeviceLang.GetSystemLanguage).
GOSR_EXPORT char* GetSystemLanguage()
{
    char value[PROP_VALUE_MAX] = "";
    if (__system_property_get("persist.sys.locale", value) <= 0)
        __system_property_get("ro.product.locale", value);
    if (!value[0])
        strcpy(value, "en-US");
    return MarshalString(value);
}
#endif

#ifdef GOSR_PLUGIN_SPFXUnityPlugin
// Answers like the game's SPFXPluginNull.
namespace
{
    void RenderEvent(int) {}
    struct Instance { int unused; };
}
GOSR_EXPORT void* SPFX_PLUGIN_GetRenderingCallback() { return reinterpret_cast<void*>(&RenderEvent); }
GOSR_EXPORT int SPFX_PLUGIN_Initialize(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, const uint8_t*, int, int, int, int, int, float, int, float, int) { return 0; }
GOSR_EXPORT void SPFX_PLUGIN_Uninitialize() {}
GOSR_EXPORT bool SPFX_PLUGIN_IsTryRendering(int, int, int) { return false; }
GOSR_EXPORT void SPFX_PLUGIN_ForceUpdate() {}
GOSR_EXPORT const char* SPFX_PLUGIN_GetVersionString() { return "GoSRecomp Android stub"; }
GOSR_EXPORT void SPFX_PLUGIN_SetLowSpecMode(int) {}
GOSR_EXPORT int SPFX_PLUGIN_GetLowSpecMode() { return 0; }
GOSR_EXPORT uint32_t SPFX_PLUGIN_GetResourceLoaderQueueCount() { return 0; }
GOSR_EXPORT void SPFX_PLUGIN_SetDistanceFadeEnable(int) {}
GOSR_EXPORT int SPFX_PLUGIN_GetDistanceFadeEnable() { return 0; }
GOSR_EXPORT void SPFX_PLUGIN_SetGroundHeight(float) {}
GOSR_EXPORT void SPFX_PLUGIN_SetZSortEnable(int, int) {}
GOSR_EXPORT void SPFX_PLUGIN_SetDrawParameter(int, void*) {}
GOSR_EXPORT void SPFX_PLUGIN_Update(float, void*) {}
GOSR_EXPORT void* SPFX_PLUGIN_Instance_Create(const uint8_t*, const uint8_t*, int, int, int, void**, int, int) { return new Instance(); }
GOSR_EXPORT void SPFX_PLUGIN_Instance_Delete(void* h) { delete static_cast<Instance*>(h); }
GOSR_EXPORT uint32_t SPFX_PLUGIN_Instance_GetSystemMemory(void*) { return 0; }
GOSR_EXPORT uint32_t SPFX_PLUGIN_Instance_GetVideoMemory(void*) { return 0; }
GOSR_EXPORT int SPFX_PLUGIN_Instance_GetStatus(void*) { return 1; }
GOSR_EXPORT void SPFX_PLUGIN_Instance_Play(void*, int) {}
GOSR_EXPORT void SPFX_PLUGIN_Instance_Stop(void*) {}
GOSR_EXPORT void SPFX_PLUGIN_Instance_Pause(void*) {}
GOSR_EXPORT int SPFX_PLUGIN_Instance_IsPlaying(void*) { return 0; }
GOSR_EXPORT void SPFX_PLUGIN_Instance_SetUpdateStopAtHide(void*, int) {}
GOSR_EXPORT void SPFX_PLUGIN_Instance_SetViewCullingBoundingSphere(void*, int, float) {}
GOSR_EXPORT void SPFX_PLUGIN_Instance_SetCullingMask(void*, int) {}
GOSR_EXPORT void SPFX_PLUGIN_Instance_SetDrawLayer(void*, int) {}
GOSR_EXPORT void SPFX_PLUGIN_Instance_SetDrawPriority(void*, int) {}
GOSR_EXPORT void SPFX_PLUGIN_Instance_SetDepthBiasOffset(void*, float) {}
GOSR_EXPORT void SPFX_PLUGIN_Instance_SetDepthBiasScale(void*, float) {}
GOSR_EXPORT void SPFX_PLUGIN_Instance_SetFogInfluence(void*, float) {}
GOSR_EXPORT int SPFX_PLUGIN_Instance_KickTrigger(void*, int) { return 1; }
GOSR_EXPORT int SPFX_PLUGIN_Instance_FrameSkip(void*, int) { return 0; }
GOSR_EXPORT void SPFX_PLUGIN_Instance_SetLabelTexture(void*, const char*, void*, int) {}
GOSR_EXPORT void SPFX_PLUGIN_Instance_SetShow(void*, int, int) {}
GOSR_EXPORT void SPFX_PLUGIN_Instance_SetTransform(void*, const float*, int, int, int) {}
GOSR_EXPORT void SPFX_PLUGIN_Instance_SetBinderAttachPointTransform(void*, int, const uint8_t*, const float*) {}
GOSR_EXPORT void SPFX_PLUGIN_Instance_SetTargetTransform(void*, const float*) {}
GOSR_EXPORT void SPFX_PLUGIN_Instance_SetColor(void*, float, float, float, float) {}
GOSR_EXPORT void SPFX_PLUGIN_Instance_SetSpeed(void*, float) {}
GOSR_EXPORT void SPFX_PLUGIN_SetFogParameters(int, float, float, float, float, float, float) {}
GOSR_EXPORT int SPFX_PLUGIN_Instance_GetBoundingSphere(void*, void*) { return 0; }
GOSR_EXPORT void SPFX_PLUGIN_GetMemoryInfo(void* info) { if (info) memset(info, 0, 8); }
GOSR_EXPORT void SPFX_PLUGIN_EnableUnityCollision(int) {}
GOSR_EXPORT void* SPFX_PLUGIN_GetCollisionData(void*) { return nullptr; }
GOSR_EXPORT void SPFX_PLUGIN_EndCollisionData(void*, int, void*) {}
GOSR_EXPORT void SPFX_PLUGIN_EnableSoundCallback(int) {}
GOSR_EXPORT void* SPFX_PLUGIN_GetSoundData(void*) { return nullptr; }
GOSR_EXPORT void SPFX_PLUGIN_EndSoundData(void*) {}
GOSR_EXPORT void SPFX_PLUGIN_EnableEventCallback(int) {}
GOSR_EXPORT void* SPFX_PLUGIN_GetClipEventData(void*) { return nullptr; }
GOSR_EXPORT void SPFX_PLUGIN_EndClipEventData(void*) {}
GOSR_EXPORT void SPFX_PLUGIN_EnablePositionNotificationCallback(int) {}
GOSR_EXPORT void* SPFX_PLUGIN_GetPositionNotificationData(void*) { return nullptr; }
GOSR_EXPORT void SPFX_PLUGIN_EndPositionNotificationData(void*) {}
GOSR_EXPORT const char* SPFX_PLUGIN_GetProfileString() { return ""; }
GOSR_EXPORT void SPFX_PLUGIN_GetProfileValue(int, void* data) { if (data) memset(data, 0, 8); }
GOSR_EXPORT void SPFX_PLUGIN_ResetProfilingMaxMemory() {}
#endif

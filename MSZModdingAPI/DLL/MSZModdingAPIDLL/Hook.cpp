#include "Hook.h"
#include "Offsets.h"
#include "API.h"
#include "IL2CPP_Helper.h"
#include "Scanner.h"
#include <algorithm>
#include <core.hpp> // CellHook

namespace Hook {
    namespace Unity {
        // --- IL2CPP Definitions ---
        il2cpp_resolve_icall_t ResolveICall = nullptr;
        UnityInstantiate_t Instantiate = nullptr;
        il2cpp_domain_get_t domain_get = nullptr;
        il2cpp_domain_assembly_open_t assembly_open = nullptr;
        il2cpp_assembly_get_image_t assembly_get_image = nullptr;
        il2cpp_class_from_name_t class_from_name = nullptr;
        il2cpp_class_get_method_from_name_t class_get_method_from_name = nullptr;
        il2cpp_runtime_invoke_t runtime_invoke = nullptr;
        il2cpp_string_new_t il2cpp_string_new = nullptr;
        il2cpp_field_get_offset_t field_get_offset = nullptr;
        il2cpp_class_get_field_from_name_t class_get_field_from_name = nullptr;
        il2cpp_class_get_type_t class_get_type = nullptr;
        il2cpp_type_get_object_t type_get_object = nullptr;
        il2cpp_object_new_t il2cpp_object_new = nullptr;

        // --- Unity Engine Definitions ---
        GetMainCamera_t GetMainCamera = nullptr;
        ScreenPointToRay_t ScreenPointToRay = nullptr;
        Raycast_t Raycast = nullptr;
        GetGameObject_t GetGameObject = nullptr;
        Destroy_t Destroy = nullptr;
        GetMousePosition_t GetMousePosition = nullptr;
        RegisterLogCallback_t RegisterLogCallback = nullptr;
        LogToConsole_t UnityLog = nullptr;
        SetGlobalFloat_t oSetGlobalFloat = nullptr;
        get_position_Injected_t GetTransformPosition = nullptr;
        get_transform_t GetTransform = nullptr;
        get_forward_Injected_t GetTransformForward = nullptr;
        get_rotation_Injected_t GetTransformRotation = nullptr;
        get_scene_t GetScene = nullptr;
        get_defaultPhysicsScene_t get_defaultPhysicsScene = nullptr;
        FindObjectFromInstanceID_t FindObjectFromInstanceID = nullptr;
        GetComponent_t GetComponent = nullptr;
        GetType_t GetType = nullptr;
        set_position_Injected_t SetTransformPosition = nullptr;
        set_rotation_Injected_t SetTransformRotation = nullptr;
        set_velocity_Injected_t SetRigidbodyVelocity = nullptr;
        set_useGravity_t SetRigidbodyGravity = nullptr;
        get_timeScale_t GetTimeScale = nullptr;
        set_timeScale_t SetTimeScale = nullptr;
        set_useGravity_t SetUseGravity = nullptr;
        set_isKinematic_t SetIsKinematic = nullptr;
        LoadScene_t LoadScene = nullptr;

        void SetGlobalFloat_Hook(int nameString, float value) {
            MSZ_API::ProcessMainThreadTasks();
            oSetGlobalFloat(nameString, value);
        }
    }

    char g_GameDir[MAX_PATH] = { 0 };

    void KillGame(const char* reason) {
        char msg[512];
        sprintf_s(msg, "SECURITY VIOLATION DETECTED!\n\nReason: %s\n\nThe game process will now terminate to protect your system.", reason);
        MessageBoxA(NULL, msg, "Security Guard", MB_OK | MB_ICONHAND | MB_SYSTEMMODAL);
        TerminateProcess(GetCurrentProcess(), 1);
    }

    bool IsPathSafe(LPCSTR filePath) {
        if (!filePath) return true;

        char fullPath[MAX_PATH];
        if (GetFullPathNameA(filePath, MAX_PATH, fullPath, NULL) == 0) return false;

        std::string sPath = fullPath;
        std::transform(sPath.begin(), sPath.end(), sPath.begin(), ::tolower);

        // --- THE EXPANDED WHITELIST ---
        std::string sGameDir = g_GameDir;
        std::transform(sGameDir.begin(), sGameDir.end(), sGameDir.begin(), ::tolower);
        if (sPath.find(sGameDir) == 0) return true;

        if (sPath.find("c:\\windows\\") == 0) return true;
        if (sPath.find("c:\\programdata\\") == 0) return true;

        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        std::string sTemp = tempPath;
        std::transform(sTemp.begin(), sTemp.end(), sTemp.begin(), ::tolower);
        if (sPath.find(sTemp) == 0) return true;

        if (sPath.find("program files") != std::string::npos) return true;

        return false;
    }

    // --- TYPE DEFINITIONS ---
    typedef HINSTANCE(WINAPI* ShellExecuteA_t)(HWND, LPCSTR, LPCSTR, LPCSTR, LPCSTR, INT);
    typedef HRESULT(WINAPI* URLDownloadToFileA_t)(LPUNKNOWN, LPCSTR, LPCSTR, DWORD, LPUNKNOWN);
    typedef HANDLE(WINAPI* CreateFileA_t)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
    typedef int       (WINAPI* connect_t)(SOCKET, const struct sockaddr*, int);

    // --- ORIGINAL POINTERS ---
    ShellExecuteA_t      fpShellExecuteA = nullptr;
    URLDownloadToFileA_t fpURLDownloadToFileA = nullptr;
    CreateFileA_t        fpCreateFileA = nullptr;
    connect_t            fpConnect = nullptr;

    // --- SECURITY TRAPS ---
    HINSTANCE WINAPI Detour_ShellExecuteA(HWND hwnd, LPCSTR op, LPCSTR file, LPCSTR params, LPCSTR dir, INT show) {
        std::string sFile = file ? file : "";
        std::string lower = sFile;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (lower.find("http://") != 0 && lower.find("https://") != 0) {
            KillGame(("Mod attempted to execute non-web resource: " + sFile).c_str());
            return 0;
        }
        return fpShellExecuteA(hwnd, op, file, params, dir, show);
    }

    HRESULT WINAPI Detour_URLDownloadToFileA(LPUNKNOWN pCaller, LPCSTR szURL, LPCSTR szFileName, DWORD dwReserved, LPUNKNOWN lpfnCB) {
        KillGame(("Mod attempted to download file: " + std::string(szURL)).c_str());
        return E_FAIL;
    }

    HANDLE WINAPI Detour_CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
        if (!IsPathSafe(lpFileName)) {
            KillGame(("Mod attempted to read file outside Game/System folder:\n" + std::string(lpFileName)).c_str());
            return INVALID_HANDLE_VALUE;
        }
        return fpCreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }

    int WINAPI Detour_Connect(SOCKET s, const struct sockaddr* name, int namelen) {
        KillGame("Mod attempted to open a raw network connection (Winsock).");
        return -1;
    }

    namespace kiriMoveBasic {
        void* lastInstance = nullptr;
        float* speedPtr = nullptr;
        typedef void(__stdcall* ToggleMovement_t)(void* instance, bool enable, bool smoothSpeed);
        ToggleMovement_t ToggleMovement = nullptr;
        void* (__stdcall* oUpdate)(void*);

        void __stdcall Update(void* obj) {
            if (obj) {
                lastInstance = obj;
                uintptr_t walkSpeedOffset = Scanner::Cache["kiriMoveBasic::walkSpeed"];
                speedPtr = (float*)((uintptr_t)obj + walkSpeedOffset);

                if (MSZ_API::Player::originalSpeed == 0.0f)
                    MSZ_API::Player::originalSpeed = *speedPtr;

                if (MSZ_API::Player::isSpeedModified)
                    *speedPtr = MSZ_API::Player::currentSpeed;
                else
                    *speedPtr = MSZ_API::Player::originalSpeed;
            }
            oUpdate(obj);
        }
    }

    void initPointers(uintptr_t gameAssembly) {
        kiriMoveBasic::ToggleMovement = (kiriMoveBasic::ToggleMovement_t)(gameAssembly + 0x3C9D50);
    }

    void DebugDump(const char* name, void* addr) {
        unsigned char* p = (unsigned char*)addr;
        printf("[DEBUG] %s at %p: %02X %02X %02X %02X %02X %02X\n",
            name, addr, p[0], p[1], p[2], p[3], p[4], p[5]);
    }

    bool InitAll(uintptr_t gameAssembly) {
        static cell::transaction game_hooks;
        static cell::transaction security_hooks;

        HMODULE gameAsm = GetModuleHandleA("GameAssembly.dll");
        if (!gameAsm) {
            LogE("GameAssembly.dll not found!");
            return false;
        }

        // --- Helper Macros ---
#define GET_EXPORT(var, type, name) var = (type)GetProcAddress(gameAsm, name); if (!var) { LogE("Failed to find Export: %s", name); return false; }
#define GET_ICALL(var, type, name) var = (type)Unity::ResolveICall(name); if (!var) { LogE("Failed to resolve ICall: %s", name); return false; }

        GET_EXPORT(Unity::ResolveICall, Unity::il2cpp_resolve_icall_t, "il2cpp_resolve_icall");
        GET_EXPORT(Unity::domain_get, Unity::il2cpp_domain_get_t, "il2cpp_domain_get");
        GET_EXPORT(Unity::assembly_open, Unity::il2cpp_domain_assembly_open_t, "il2cpp_domain_assembly_open");
        GET_EXPORT(Unity::assembly_get_image, Unity::il2cpp_assembly_get_image_t, "il2cpp_assembly_get_image");
        GET_EXPORT(Unity::class_from_name, Unity::il2cpp_class_from_name_t, "il2cpp_class_from_name");
        GET_EXPORT(Unity::class_get_method_from_name, Unity::il2cpp_class_get_method_from_name_t, "il2cpp_class_get_method_from_name");
        GET_EXPORT(Unity::runtime_invoke, Unity::il2cpp_runtime_invoke_t, "il2cpp_runtime_invoke");
        GET_EXPORT(Unity::class_get_field_from_name, Unity::il2cpp_class_get_field_from_name_t, "il2cpp_class_get_field_from_name");
        GET_EXPORT(Unity::field_get_offset, Unity::il2cpp_field_get_offset_t, "il2cpp_field_get_offset");
        GET_EXPORT(Unity::class_get_type, Unity::il2cpp_class_get_type_t, "il2cpp_class_get_type");
        GET_EXPORT(Unity::type_get_object, Unity::il2cpp_type_get_object_t, "il2cpp_type_get_object");
        GET_EXPORT(Unity::il2cpp_object_new, Unity::il2cpp_object_new_t, "il2cpp_object_new");

        Unity::il2cpp_string_new = (Unity::il2cpp_string_new_t)GetProcAddress(gameAsm, "il2cpp_string_new");
        if (!Unity::il2cpp_string_new) {
            Unity::il2cpp_string_new = (Unity::il2cpp_string_new_t)GetProcAddress(gameAsm, "il2cpp_string_new_utf16");
        }
        if (!Unity::il2cpp_string_new) {
            LogE("CRITICAL: Failed to find il2cpp_string_new!");
            return false;
        }

        if (Unity::ResolveICall) {
            using namespace Unity;
            GET_ICALL(Instantiate, UnityInstantiate_t, "UnityEngine.Object::Internal_InstantiateSingle_Injected(UnityEngine.Object,UnityEngine.Vector3&,UnityEngine.Quaternion&)");
            GET_ICALL(GetMainCamera, GetMainCamera_t, "UnityEngine.Camera::get_main()");
            GET_ICALL(ScreenPointToRay, ScreenPointToRay_t, "UnityEngine.Camera::ScreenPointToRay_Injected(UnityEngine.Vector2&,UnityEngine.Camera/MonoOrStereoscopicEye,UnityEngine.Ray&)");
            GET_ICALL(Raycast, Raycast_t, "UnityEngine.PhysicsScene::Internal_Raycast_Injected(UnityEngine.PhysicsScene&,UnityEngine.Ray&,System.Single,UnityEngine.RaycastHit&,System.Int32,UnityEngine.QueryTriggerInteraction)");
            GET_ICALL(get_defaultPhysicsScene, get_defaultPhysicsScene_t, "UnityEngine.Physics::get_defaultPhysicsScene_Injected(UnityEngine.PhysicsScene&)");
            GET_ICALL(Destroy, Destroy_t, "UnityEngine.Object::Destroy(UnityEngine.Object)");
            GET_ICALL(GetGameObject, GetGameObject_t, "UnityEngine.Component::get_gameObject()");
            GET_ICALL(GetTransform, get_transform_t, "UnityEngine.Component::get_transform()");
            GET_ICALL(GetTransformPosition, get_position_Injected_t, "UnityEngine.Transform::get_position_Injected(UnityEngine.Vector3&)");
            GET_ICALL(GetTransformRotation, get_rotation_Injected_t, "UnityEngine.Transform::get_rotation_Injected(UnityEngine.Quaternion&)");
            GET_ICALL(FindObjectFromInstanceID, FindObjectFromInstanceID_t, "UnityEngine.Object::FindObjectFromInstanceID(System.Int32)");
            GET_ICALL(GetMousePosition, GetMousePosition_t, "UnityEngine.Input::get_mousePosition_Injected(UnityEngine.Vector3&)");
            GET_ICALL(Unity::SetTransformPosition, Unity::set_position_Injected_t, "UnityEngine.Transform::set_position_Injected(UnityEngine.Vector3&)");
            GET_ICALL(Unity::SetTransformRotation, Unity::set_rotation_Injected_t, "UnityEngine.Transform::set_rotation_Injected(UnityEngine.Quaternion&)");
            GET_ICALL(Unity::GetComponent, Unity::GetComponent_t, "UnityEngine.GameObject::GetComponent(System.Type)");
            GET_ICALL(Unity::SetRigidbodyVelocity, Unity::set_velocity_Injected_t, "UnityEngine.Rigidbody::set_velocity_Injected(UnityEngine.Vector3&)");
            GET_ICALL(Unity::SetRigidbodyGravity, Unity::set_useGravity_t, "UnityEngine.Rigidbody::set_useGravity(System.Boolean)");
            GET_ICALL(GetTimeScale, get_timeScale_t, "UnityEngine.Time::get_timeScale()");
            GET_ICALL(SetTimeScale, set_timeScale_t, "UnityEngine.Time::set_timeScale(System.Single)");
            GET_ICALL(SetUseGravity, set_useGravity_t, "UnityEngine.Rigidbody::set_useGravity(System.Boolean)");
            GET_ICALL(SetIsKinematic, set_isKinematic_t, "UnityEngine.Rigidbody::set_isKinematic(System.Boolean)");

            Scanner::Initialize();

            // =========================================================
            // Game Hooks
            // =========================================================
            kiriMoveBasic::ToggleMovement = (kiriMoveBasic::ToggleMovement_t)Scanner::Cache["kiriMoveBasic::ToggleMovement"];
            void* updateAddr = (void*)Scanner::Cache["kiriMoveBasic::Update"];
            if (updateAddr) {
                game_hooks.add(updateAddr, &kiriMoveBasic::Update, (LPVOID*)&kiriMoveBasic::oUpdate);
            }
            else {
                LogE("Critical: Scanner could not find address for kiriMoveBasic::Update");
            }

            auto setGlobalFloatAddr = Unity::ResolveICall("UnityEngine.Shader::SetGlobalFloatImpl(System.Int32,System.Single)");
            if (!setGlobalFloatAddr) setGlobalFloatAddr = Unity::ResolveICall("UnityEngine.Shader::SetGlobalFloat(System.Int32,System.Single)");
            if (setGlobalFloatAddr) {
                game_hooks.add(setGlobalFloatAddr, &Unity::SetGlobalFloat_Hook, (LPVOID*)&Unity::oSetGlobalFloat);
            }

            // COMMIT GAME HOOKS
            if (game_hooks.commit() != cell::status::success) {
                LogE("[-] GAME HOOKS FAILED! Cheat functionality might be broken.");
            }
            else {
                LogI("[+] Game Hooks active. Menu & ESP enabled.");
                MSZ_API::Initialized = true; // IMPORTANT: Let the menu open!
            }

            // =========================================================
            // Security Hooks
            // =========================================================
            GetCurrentDirectoryA(MAX_PATH, g_GameDir);
            LogI("[SECURITY] Sandbox Active. Root: %s", g_GameDir);

            //DebugDump("CreateFileA", (void*)&CreateFileA);
            //DebugDump("ShellExecuteA", (void*)&ShellExecuteA);

            // Add hooks to security batch
            security_hooks.add(&ShellExecuteA, &Detour_ShellExecuteA, &fpShellExecuteA);
            security_hooks.add(&CreateFileA, &Detour_CreateFileA, &fpCreateFileA);

            HMODULE hUrlMon = LoadLibraryA("urlmon.dll");
            if (hUrlMon) {
                void* p = (void*)GetProcAddress(hUrlMon, "URLDownloadToFileA");
                if (p) security_hooks.add(p, &Detour_URLDownloadToFileA, (LPVOID*)&fpURLDownloadToFileA);
            }

            HMODULE hWinsock = LoadLibraryA("ws2_32.dll");
            if (hWinsock) {
                void* p = (void*)GetProcAddress(hWinsock, "connect");
                if (p) security_hooks.add(p, &Detour_Connect, (LPVOID*)&fpConnect);
            }

            // COMMIT SECURITY HOOKS
            if (security_hooks.commit() != cell::status::success) {
                LogW("[-] Warning: One or more Security Hooks failed.");
                LogW("    (Likely CreateFileA due to API forwarding. Game Hooks are safe.)");
            }
            else {
                LogI("[+] Security Hooks active.");
            }
        }
        return true;
    }
}
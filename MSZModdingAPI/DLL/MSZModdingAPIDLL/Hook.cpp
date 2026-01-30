#include "Hook.h"
#include "MinHook.h"
#include "Offsets.h"
#include "API.h"
#include "IL2CPP_Helper.h"
#include "Scanner.h"

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
        void SetGlobalFloat_Hook(int nameString, float value){
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

        // 1. Game Directory
        std::string sGameDir = g_GameDir;
        std::transform(sGameDir.begin(), sGameDir.end(), sGameDir.begin(), ::tolower);
        if (sPath.find(sGameDir) == 0) return true;

        // 2. Windows System Files (Required for DirectX/Drivers)
        if (sPath.find("c:\\windows\\") == 0) return true;

        // 3. NEW: ProgramData (Used by NVIDIA ShadowPlay, Steam, etc.)
        if (sPath.find("c:\\programdata\\") == 0) return true;

        // 4. NEW: Temp Folder (Used by many legitimate libraries)
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        std::string sTemp = tempPath;
        std::transform(sTemp.begin(), sTemp.end(), sTemp.begin(), ::tolower);
        if (sPath.find(sTemp) == 0) return true;

        // 5. NEW: Common Redistributables / Steam
        if (sPath.find("program files") != std::string::npos) {
            // You can be more specific here if you want to block everything 
            // except the Steam overlay, but usually Program Files is safe-ish 
            // for READ access.
            return true;
        }

        // --- THE CRITICAL BLOCKS ---
        // We still block the most sensitive areas:
        // C:\Users\<Name>\Documents
        // C:\Users\<Name>\AppData (where browser cookies/Discord tokens live)
        // C:\Users\<Name>\Desktop

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

    // --- TRAP 1: ShellExecute (Stops Execution) ---
    HINSTANCE WINAPI Detour_ShellExecuteA(HWND hwnd, LPCSTR op, LPCSTR file, LPCSTR params, LPCSTR dir, INT show) {
        std::string sFile = file ? file : "";
        std::string lower = sFile;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        // Strict Whitelist: Only allow Web Links
        if (lower.find("http://") != 0 && lower.find("https://") != 0) {
            KillGame(("Mod attempted to execute non-web resource: " + sFile).c_str());
            return 0;
        }
        return fpShellExecuteA(hwnd, op, file, params, dir, show);
    }

    // --- TRAP 2: URLDownloadToFile (Stops Downloading) ---
    HRESULT WINAPI Detour_URLDownloadToFileA(LPUNKNOWN pCaller, LPCSTR szURL, LPCSTR szFileName, DWORD dwReserved, LPUNKNOWN lpfnCB) {
        KillGame(("Mod attempted to download file: " + std::string(szURL)).c_str());
        return E_FAIL;
    }

    // --- TRAP 3: CreateFile (The File Sandbox) ---
    HANDLE WINAPI Detour_CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {

        // Only check if they are trying to READ or OPEN
        // (We usually don't care about creating temp files, but reading is dangerous)
        if (!IsPathSafe(lpFileName)) {
            KillGame(("Mod attempted to read file outside Game/System folder:\n" + std::string(lpFileName)).c_str());
            return INVALID_HANDLE_VALUE;
        }

        return fpCreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }

    // --- TRAP 4: Winsock Connect (Stops IP Logging) ---
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

    bool InitAll(uintptr_t gameAssembly) {
        HMODULE gameAsm = GetModuleHandleA("GameAssembly.dll");
        if (!gameAsm) {
            LogE("GameAssembly.dll not found!");
            return false;
        }

        // --- Helper Macro for GetProcAddress ---
#define GET_EXPORT(var, type, name) \
            var = (type)GetProcAddress(gameAsm, name); \
            if (!var) { \
                LogE("Failed to find Export: %s", name); \
                return false; \
            }

        // 1. Resolve basic IL2CPP Exports
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

        // Fallback for newer Unity versions
        if (!Unity::il2cpp_string_new) {
            LogW("il2cpp_string_new not found, trying il2cpp_string_new_utf16...");
            Unity::il2cpp_string_new = (Unity::il2cpp_string_new_t)GetProcAddress(gameAsm, "il2cpp_string_new_utf16");
        }

        if (!Unity::il2cpp_string_new) {
            LogE("CRITICAL: Failed to find any variant of il2cpp_string_new!");
            return false;
        }
        // --- Helper Macro for ResolveICall ---
#define GET_ICALL(var, type, name) \
            var = (type)Unity::ResolveICall(name); \
            if (!var) { \
                LogE("Failed to resolve ICall: %s", name); \
                LogE("Mods won't work, Report this to c___s on discord."); \
                return false; \
            }

        // 2. Resolve Internal Engine Functions
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
                         
            // 3. Initialize Scanner (Finds all addresses/offsets dynamically)
            Scanner::Initialize();

            kiriMoveBasic::ToggleMovement = (kiriMoveBasic::ToggleMovement_t)Scanner::Cache["kiriMoveBasic::ToggleMovement"];

            uintptr_t findObjAddr = Scanner::GetMethod("UnityEngine.CoreModule", "UnityEngine", "Object", "FindObjectOfType", 2);

            if (MH_Initialize() == MH_OK) {
                void* updateAddr = (void*)Scanner::Cache["kiriMoveBasic::Update"];

                if (updateAddr) {
                    MH_CreateHook(updateAddr, &kiriMoveBasic::Update, (LPVOID*)&kiriMoveBasic::oUpdate);
                    MH_EnableHook(updateAddr);
                    LogI("kiriMoveBasic::Update Hooked successfully.");
                }
                else {
                    LogE("Critical: Scanner could not find address for kiriMoveBasic::Update");
                    return false;
                }

                auto setGlobalFloatAddr = Unity::ResolveICall("UnityEngine.Shader::SetGlobalFloatImpl(System.Int32,System.Single)");
                if (!setGlobalFloatAddr) {
                    setGlobalFloatAddr = Unity::ResolveICall("UnityEngine.Shader::SetGlobalFloat(System.Int32,System.Single)");
                }

                if (setGlobalFloatAddr) {
                    MH_CreateHook(setGlobalFloatAddr, &Unity::SetGlobalFloat_Hook, (LPVOID*)&Unity::oSetGlobalFloat);
                    MH_EnableHook(setGlobalFloatAddr);
                    LogI("Heartbeat (SetGlobalFloat) Hooked successfully.");
                }
                else {
                    LogW("Warning: Could not find SetGlobalFloat. Main thread tasks might not process!");
                }

                LogI("MinHook: Successfully hooked kiriMoveBasic::Update");

                GetCurrentDirectoryA(MAX_PATH, g_GameDir);
                LogI("[SECURITY] Sandbox Active. Root: %s", g_GameDir);

                // 1. Install Hooks
                MH_CreateHook(&ShellExecuteA, &Detour_ShellExecuteA, (LPVOID*)&fpShellExecuteA);
                MH_EnableHook(&ShellExecuteA);

                // Hook CreateFileA (Kernel32.dll)
                MH_CreateHook(&CreateFileA, &Detour_CreateFileA, (LPVOID*)&fpCreateFileA);
                MH_EnableHook(&CreateFileA);

                // Load extra DLLs to hook them
                HMODULE hUrlMon = LoadLibraryA("urlmon.dll");
                if (hUrlMon) {
                    void* p = (void*)GetProcAddress(hUrlMon, "URLDownloadToFileA");
                    if (p) {
                        MH_CreateHook(p, &Detour_URLDownloadToFileA, (LPVOID*)&fpURLDownloadToFileA);
                        MH_EnableHook(p);
                    }
                }

                HMODULE hWinsock = LoadLibraryA("ws2_32.dll");
                if (hWinsock) {
                    void* p = (void*)GetProcAddress(hWinsock, "connect");
                    if (p) {
                        MH_CreateHook(p, &Detour_Connect, (LPVOID*)&fpConnect);
                        MH_EnableHook(p);
                    }
                }

                MSZ_API::Initialized = true;
                return true;
            }
        }
        LogI("Minhook didn't initialized successfully, mods won't work.");
        return false;
    }
}
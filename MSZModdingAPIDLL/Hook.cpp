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
        DrawLine_t DrawLine = nullptr;
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
            GET_ICALL(DrawLine, DrawLine_t, "UnityEngine.Debug::DrawLine_Injected(UnityEngine.Vector3&,UnityEngine.Vector3&,UnityEngine.Color&,System.Single,System.Boolean)");
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
                MSZ_API::Initialized = true;
                return true;
            }
        }
        LogI("Minhook didn't initialized successfully, mods won't work.");
        return false;
    }
}
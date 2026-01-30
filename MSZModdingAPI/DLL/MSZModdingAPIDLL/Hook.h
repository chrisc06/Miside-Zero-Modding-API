#pragma once
#include <windows.h>
#include "API.h"
namespace Hook {
    namespace Unity {
        // --- IL2CPP Typedefs ---
        typedef void* (*il2cpp_resolve_icall_t)(const char* name);
        typedef void* (*UnityInstantiate_t)(void* obj, Vector3 pos, Quaternion rot);
        typedef void* (*il2cpp_domain_get_t)();
        typedef void* (*il2cpp_domain_assembly_open_t)(void* domain, const char* name);
        typedef void* (*il2cpp_assembly_get_image_t)(void* assembly);
        typedef void* (*il2cpp_class_from_name_t)(void* image, const char* namespaze, const char* name);
        typedef void* (*il2cpp_class_get_method_from_name_t)(void* klass, const char* name, int argsCount);
        typedef void* (*il2cpp_runtime_invoke_t)(void* method, void* obj, void** params, void** exc);
        typedef void* (*il2cpp_string_new_t)(const char* str);
        typedef void* (__stdcall* il2cpp_class_get_field_from_name_t)(void* klass, const char* name);
        typedef size_t(__stdcall* il2cpp_field_get_offset_t)(void* field);
        typedef void* (__stdcall* il2cpp_class_get_type_t)(void* klass);
        typedef void* (__stdcall* il2cpp_type_get_object_t)(void* type);
        typedef void* (__stdcall* il2cpp_object_new_t)(void* klass);

        extern il2cpp_resolve_icall_t ResolveICall;
        extern UnityInstantiate_t Instantiate;
        extern il2cpp_domain_get_t domain_get;
        extern il2cpp_domain_assembly_open_t assembly_open;
        extern il2cpp_assembly_get_image_t assembly_get_image;
        extern il2cpp_class_from_name_t class_from_name;
        extern il2cpp_class_get_method_from_name_t class_get_method_from_name;
        extern il2cpp_runtime_invoke_t runtime_invoke;
        extern il2cpp_string_new_t il2cpp_string_new;
        extern il2cpp_class_get_field_from_name_t class_get_field_from_name;
        extern il2cpp_field_get_offset_t field_get_offset;
        extern il2cpp_class_get_type_t class_get_type;
        extern il2cpp_type_get_object_t type_get_object;
        extern il2cpp_object_new_t il2cpp_object_new;

        // --- Unity Engine Typedefs ---
        typedef void* (*GetMainCamera_t)();
        typedef void (*ScreenPointToRay_t)(void* _this, Vector2* pos, int eye, Ray* outRay);
        typedef bool(__stdcall* Raycast_t)(PhysicsScene* physicsScene, Ray* ray, float distance, RaycastHit* hit, int mask, int interaction);
        typedef void* (*GetGameObject_t)(void* component);
        typedef void (*Destroy_t)(void* obj);
        typedef void (*GetMousePosition_t)(Vector3* outPos);
        typedef void (*LogToConsole_t)(void* stringObj);
        typedef void (*LogCallback)(void* condition, void* stackTrace, int type);
        typedef void (*RegisterLogCallback_t)(LogCallback handler);
        typedef void (*SendWillRenderCanvases_t)();
        typedef void (*SetGlobalFloat_t)(int nameID, float value);
        typedef void (*get_position_Injected_t)(void* _this, Vector3* outPos);
        typedef void* (*get_transform_t)(void* component);
        typedef void (*get_forward_Injected_t)(void* _this, Vector3* outForward);
        typedef void (*get_rotation_Injected_t)(void* _this, Quaternion* outRot);
        typedef void (*get_scene_t)(void* gameObject, int* outSceneHandle);
        typedef void(__stdcall* get_defaultPhysicsScene_t)(PhysicsScene* outScene);
        typedef void* (__stdcall* FindObjectFromInstanceID_t)(int instanceID);
        typedef void* (__stdcall* GetComponent_t)(void* gameObject, void* type);
        typedef void* (__stdcall* GetType_t)(void* managedStringPtr);
        typedef void(__stdcall* set_position_Injected_t)(void* transform, Vector3* pos);
        typedef void(__stdcall* set_rotation_Injected_t)(void* transform, Quaternion* rot);
        typedef void(__stdcall* set_velocity_Injected_t)(void* rigidbody, Vector3* velocity);
        typedef void(__stdcall* set_useGravity_t)(void* rigidbody, bool useGravity);
        typedef float (*get_timeScale_t)();
        typedef void (*set_timeScale_t)(float);
        typedef void (*set_useGravity_t)(void*, bool);
        typedef void (*set_isKinematic_t)(void*, bool);
        typedef void (*LoadScene_t)(const char* sceneName);

        extern GetMainCamera_t GetMainCamera;
        extern ScreenPointToRay_t ScreenPointToRay;
        extern Raycast_t Raycast;
        extern GetMousePosition_t GetMousePosition;
        extern GetGameObject_t GetGameObject;
        extern Destroy_t Destroy;
        extern LogToConsole_t UnityLog;
        extern RegisterLogCallback_t RegisterLogCallback;
        extern SetGlobalFloat_t oSetGlobalFloat;
        extern get_position_Injected_t GetTransformPosition;
        extern get_transform_t GetTransform;
        extern get_forward_Injected_t GetTransformForward;
        extern get_rotation_Injected_t GetTransformRotation;
        extern get_scene_t GetScene;
        extern get_defaultPhysicsScene_t get_defaultPhysicsScene;
        extern FindObjectFromInstanceID_t FindObjectFromInstanceID;
        extern GetComponent_t GetComponent;
        extern GetType_t GetType;
        extern set_position_Injected_t SetTransformPosition;
        extern set_rotation_Injected_t SetTransformRotation;
        extern set_velocity_Injected_t SetRigidbodyVelocity;
        extern set_useGravity_t SetRigidbodyGravity;
        extern get_timeScale_t GetTimeScale;
        extern set_timeScale_t SetTimeScale;
        extern set_useGravity_t SetUseGravity;
        extern set_isKinematic_t SetIsKinematic;
        extern LoadScene_t LoadScene;
    }

    namespace kiriMoveBasic {
        extern float* speedPtr;
        extern void* lastInstance;
        extern void* playerPtr;

        typedef void(__stdcall* ToggleMovement_t)(void* instance, bool enable, bool smoothSpeed);
        extern ToggleMovement_t ToggleMovement;
    }

    bool InitAll(uintptr_t gameAssembly);
}
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <string>
#include <windows.h>
#include "API.h"
#include "Hook.h"
#include "IL2CPP_Helper.h"
#include <metahost.h>
#include <mscoree.h>
#include <filesystem>
#include <vector>
#include <fstream>
#include <map>
#include "Scanner.h"
#include "imgui/imgui.h"
#include <mutex>

namespace MSZ_API {

    std::queue<std::function<void()>> mainThreadQueue;
    std::mutex queueMutex;
    bool Initialized = false;

    void ProcessMainThreadTasks() {
        std::unique_lock<std::mutex> lock(queueMutex);
        while (!mainThreadQueue.empty()) {
            auto task = mainThreadQueue.front();
            mainThreadQueue.pop();
            lock.unlock();
            task();
            lock.lock();
        }
    }

    void RunOnMainThread(std::function<void()> task) {
        std::lock_guard<std::mutex> lock(queueMutex);
        mainThreadQueue.push(task);
    }

    namespace Unity {
        void* GetMainCamera() {
            if (Hook::Unity::GetMainCamera) {
                return Hook::Unity::GetMainCamera();
            }
            return nullptr;
        }

        void* GetGameObject(void* componentOrCollider) {
            if (Hook::Unity::GetGameObject && componentOrCollider) {
                return Hook::Unity::GetGameObject(componentOrCollider);
            }
            return nullptr;
        }

        void* GetTransform(void* unityObject) {
            if (!unityObject) {
                LogE("[Trace] GetTransform: unityObject is NULL");
                return nullptr;
            }

            if (!Hook::Unity::GetTransform) {
                LogE("[Trace] GetTransform: Hook is missing");
                return nullptr;
            }

            // Attempt to fetch the transform
            void* result = Hook::Unity::GetTransform(unityObject);

            if (!result) {
                LogW("[Trace] get_transform(0x%p) returned NULL - Object may be initializing", unityObject);
            }

            return result;
        }

        void* GetTypeObject(const char* assemblyQualifiedName) {
            using namespace Hook::Unity;

            std::string fullName(assemblyQualifiedName);

            size_t commaPos = fullName.find(',');
            if (commaPos == std::string::npos) {
                LogE("Invalid assembly-qualified name: %s", assemblyQualifiedName);
                return nullptr;
            }

            std::string typeName = fullName.substr(0, commaPos);
            std::string assemblyName = fullName.substr(commaPos + 2);
            size_t lastDot = typeName.find_last_of('.');
            std::string namespaceName = typeName.substr(0, lastDot);
            std::string className = typeName.substr(lastDot + 1);

            LogI("Resolving Type: %s.%s from %s", namespaceName.c_str(), className.c_str(), assemblyName.c_str());

            void* domain = domain_get();
            if (!domain) {
                LogE("Failed to get IL2CPP domain");
                return nullptr;
            }

            void* assembly = assembly_open(domain, assemblyName.c_str());
            if (!assembly) {
                LogE("Failed to open assembly: %s", assemblyName.c_str());
                return nullptr;
            }
            void* image = assembly_get_image(assembly);
            if (!image) {
                LogE("Failed to get assembly image");
                return nullptr;
            }

            void* klass = class_from_name(image, namespaceName.c_str(), className.c_str());
            if (!klass) {
                LogE("Failed to find class: %s.%s", namespaceName.c_str(), className.c_str());
                return nullptr;
            }

            void* ilType = class_get_type(klass);
            if (!ilType) {
                LogE("Failed to get IL2CPP type");
                return nullptr;
            }

            void* typeObject = type_get_object(ilType);
            if (!typeObject) {
                LogE("Failed to convert to System.Type object");
                return nullptr;
            }

            LogI("Successfully resolved Type object: 0x%p", typeObject);
            return typeObject;
        }

        namespace TypeCache {
            std::map<std::string, void*> cache;

            void* Get(const char* assemblyQualifiedName) {
                auto it = cache.find(assemblyQualifiedName);
                if (it != cache.end()) {
                    return it->second;
                }

                void* typeObj = GetTypeObject(assemblyQualifiedName);
                if (typeObj) {
                    cache[assemblyQualifiedName] = typeObj;
                }
                return typeObj;
            }
        }

        Vector3 GetMousePosition() {
            Vector3 pos = { 0, 0, 0 };
            if (Hook::Unity::GetMousePosition) {
                Hook::Unity::GetMousePosition(&pos);
            }
            return pos;
        }

        Ray ScreenPointToRay(void* camera, Vector3 position) {
            Ray outRay = { {0,0,0}, {0,0,0} };
            if (Hook::Unity::ScreenPointToRay && camera) {
                Vector2 pos2D = { position.x, position.y };

                Hook::Unity::ScreenPointToRay(camera, &pos2D, 0, &outRay);
            }
            return outRay;
        }

        Vector3 GetForward(void* unityObject) {
            if (!unityObject || !Hook::Unity::GetTransformRotation) return { 0, 0, 1 };

            void* transform = Hook::Unity::GetTransform(unityObject);
            if (!transform) return { 0, 0, 1 };
            Quaternion q = { 0, 0, 0, 1 };
            Hook::Unity::GetTransformRotation(transform, &q);

            Vector3 forward;
            forward.x = 2.0f * (q.x * q.z + q.w * q.y);
            forward.y = 2.0f * (q.y * q.z - q.w * q.x);
            forward.z = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);

            return forward;
        }

        Vector3 GetPosition(void* unityObject) {
            Vector3 pos = { 0, 0, 0 };
            if (!unityObject || !Hook::Unity::GetTransformPosition) return pos;

            void* transform = Hook::Unity::GetTransform(unityObject);

            if (transform) {
                Hook::Unity::GetTransformPosition(transform, &pos);
            }
            return pos;
        }

        Vector3 GetPositionFromObject(void* unityObject) {
            if (!unityObject) return { 0, 0, 0 };
            return GetPosition(unityObject);

        }

        Ray CreateWorldRay(void* camera) {
            Ray ray;
            ray.origin = GetPosition(camera);

            ray.direction = GetForward(camera);

            return ray;
        }

        bool Raycast(Ray* ray, float distance, RaycastHit* hit, int layerMask, int interaction) {
            if (!Hook::Unity::Raycast || !Hook::Unity::get_defaultPhysicsScene) return false;

           if (abs(ray->direction.x) < 0.001f && abs(ray->direction.y) < 0.001f && abs(ray->direction.z) < 0.001f) {
                return false;
            }

            memset(hit, 0, sizeof(RaycastHit));

            PhysicsScene sceneStruct;
            sceneStruct.handle = 0;
            Hook::Unity::get_defaultPhysicsScene(&sceneStruct);
            return Hook::Unity::Raycast(&sceneStruct, ray, distance, hit, layerMask, interaction);
        }

        void* FindObjectFromInstanceID(int instanceID) {
            if (Hook::Unity::FindObjectFromInstanceID) {
                return Hook::Unity::FindObjectFromInstanceID(instanceID);
            }
            return nullptr;
        }


        void Destroy(void* unityObject) {
            if (Hook::Unity::Destroy && unityObject) {
                Hook::Unity::Destroy(unityObject);
            }
        }

        void DestroyGameObject(void* componentOrObject) {
            if (!componentOrObject) return;

            void* go = GetGameObject(componentOrObject);

            if (go) {
                Destroy(go);
            }
            else {
                Destroy(componentOrObject);
            }
        }

        void Destroy(int instanceID) {
            if (instanceID == 0) return;

            if (Hook::Unity::FindObjectFromInstanceID) {
                void* obj = Hook::Unity::FindObjectFromInstanceID(instanceID);
                if (obj) {
                    Destroy(obj);
                }
                else {
                    LogW("Destroy failed: Could not find object with ID %d", instanceID);
                }
            }
        }

        namespace World {
            void SetTimeScale(float scale) {
                if (Hook::Unity::SetTimeScale) {
                    Hook::Unity::SetTimeScale(scale);
                }
            }

            float GetTimeScale() {
                return Hook::Unity::GetTimeScale();
            }

            void LoadScene(const char* sceneName) {
                if (Hook::Unity::LoadScene) {

                    //void* il2cppStr = Hook::Unity::il2cpp_string_new(sceneName);

                   // if (il2cppStr) {
                    //    LogI("API: Calling LoadScene for '%s'", sceneName);
                        // Direct call to the methodPointer
                        Hook::Unity::LoadScene(sceneName);
                   // }
                }
                else {
                    LogE("API: LoadScene call failed. Scanner could not find the method pointer.");
                }
            }
        }

        namespace Physics {
            void SetGravity(void* rigidbody, bool enabled) {
                if (rigidbody && Hook::Unity::SetUseGravity) {
                    Hook::Unity::SetUseGravity(rigidbody, enabled);
                }
            }

            void SetKinematic(void* rigidbody, bool enabled) {
                if (rigidbody && Hook::Unity::SetIsKinematic) {
                    Hook::Unity::SetIsKinematic(rigidbody, enabled);
                }
            }
        }
    }

    namespace UI {
        std::vector<std::function<void()>> m_DrawCallbacks;
        bool m_ShowMenu = true;
        std::mutex m_RenderMutex;

        void RegisterMenu(std::function<void()> callback) {
            std::lock_guard<std::mutex> lock(m_RenderMutex);
            m_DrawCallbacks.push_back(callback);
        }

        void SetMenuOpen(bool open) { m_ShowMenu = open; }
        bool IsMenuOpen() { return m_ShowMenu; }
        void ToggleMenu() { m_ShowMenu = !m_ShowMenu; }

        bool Begin(const char* name) {
                return ImGui::Begin(name, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        }

        void End() {
            ImGui::End();
        }

        void Separator() {
            ImGui::Separator();
        }

        void SameLine() {
            ImGui::SameLine();
        }

        void Spacing() {
            ImGui::Spacing();
        }

        void Text(const char* fmt, ...) {
            va_list args;
            va_start(args, fmt);
            ImGui::TextV(fmt, args);
            va_end(args);
        }

        bool Button(const char* label) {
            return ImGui::Button(label);
        }

        bool Checkbox(const char* label, bool* v) {
            return ImGui::Checkbox(label, v);
        }

        bool SliderFloat(const char* label, float* v, float v_min, float v_max) {
            return ImGui::SliderFloat(label, v, v_min, v_max);
        }

        bool InputText(const char* label, char* buf, size_t buf_size) {
            return ImGui::InputText(label, buf, buf_size);
        }\

        namespace Internal {
            void RenderAll() {
                if (!m_ShowMenu) return;
                std::lock_guard<std::mutex> lock(m_RenderMutex);

                for (const auto& drawFunc : m_DrawCallbacks) {
                    if (drawFunc) drawFunc();
                }
            }
        }
    }

    namespace Player {
        float originalSpeed, currentSpeed;
        bool isSpeedModified;

        void* GetPlayer() {
            return Hook::kiriMoveBasic::lastInstance;
        }

        void* GetTransform() {
            void* player = GetPlayer();
            if (!player) {
                LogE("Player doesn't exist!");
                return nullptr;
            }
            return Hook::Unity::GetTransform(player);
        }

        void* GetRigidbody() {
            void* player = GetPlayer();
            if (!player) return nullptr;

            void* gameObject = MSZ_API::Unity::GetGameObject(player);
            if (!gameObject) return nullptr;

            static void* rbType = nullptr;
            if (!rbType) {
                void* domain = Hook::Unity::domain_get();
                void* assembly = Hook::Unity::assembly_open(domain, "UnityEngine.PhysicsModule");
                if (assembly) {
                    void* image = Hook::Unity::assembly_get_image(assembly);
                    void* klass = Hook::Unity::class_from_name(image, "UnityEngine", "Rigidbody");
                    if (klass) {
                        rbType = Hook::Unity::type_get_object(Hook::Unity::class_get_type(klass));
                    }
                }
            }

            if (rbType && Hook::Unity::GetComponent) {
                return Hook::Unity::GetComponent(gameObject, rbType);
            }
            return nullptr;
        }

        void SetVelocity(Vector3 velocity) {
            void* rb = GetRigidbody();
            if (!rb || !Hook::Unity::SetRigidbodyVelocity) return;
            Hook::Unity::SetRigidbodyVelocity(rb, &velocity);
        }

        void Teleport(Vector3 position) {
            void* trans = GetTransform();
            if (!trans || !Hook::Unity::SetTransformPosition) return;
            Hook::Unity::SetTransformPosition(trans, &position);
        }

        void SetPlayerMovementSpeed(float speed) {
            currentSpeed = speed;
            isSpeedModified = true;
        }

        float GetPlayerMovementSpeed() {
            if (Hook::kiriMoveBasic::speedPtr != nullptr) {
                return *Hook::kiriMoveBasic::speedPtr;
            }   
            return 0;
        }

        void ToggleMovement(bool enable, bool smoothSpeed) {
            if (Hook::kiriMoveBasic::lastInstance) {
                Hook::kiriMoveBasic::ToggleMovement(Hook::kiriMoveBasic::lastInstance, enable, smoothSpeed);
            }
            else {
                LogE("KiriMoveBasic object missing.");
            }
        }
    }
}
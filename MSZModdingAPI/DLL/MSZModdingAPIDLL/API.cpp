#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <Psapi.h> 
#pragma comment(lib, "Psapi.lib") 

#include <iostream>
#include <string>
#include "API.h"
#include "Hook.h"
#include "IL2CPP_Helper.h"
#include <filesystem>
#include <vector>
#include <fstream>
#include <map>
#include <unordered_map>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <algorithm>
#include "Scanner.h"
#include "imgui/imgui.h"
#include <mutex>

namespace MSZ_API {

    std::queue<std::function<void()>> mainThreadQueue;
    std::mutex queueMutex;
    bool Initialized = false;

    static std::mutex g_modListMutex;
    static std::vector<ActiveMod> g_cachedMods;
    static bool g_modsScanned = false;

    std::vector<ActiveMod> GetLoadedMods() {
        std::vector<ActiveMod> mods;
        HMODULE hMods[1024];
        HANDLE hProcess = GetCurrentProcess();
        DWORD cbNeeded;

        if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
            unsigned int count = cbNeeded / sizeof(HMODULE);

            for (unsigned int i = 0; i < count; i++){
                typedef MSZ_ModInfo(*GetInfo_t)();
                GetInfo_t getInfo = (GetInfo_t)GetProcAddress(hMods[i], "MSZ_GetModInfo");

                if (getInfo) {
                    MSZ_ModInfo info = getInfo();

                    ActiveMod mod;
                    mod.hModule = hMods[i];
                    mod.info = info;

                    mods.push_back(mod);
                }
            }
        }
        return mods;
    }

    std::vector<ActiveMod> GetLoadedModsCached(bool refresh) {
        std::lock_guard<std::mutex> lock(g_modListMutex);
        if (!g_modsScanned || refresh) {
            g_cachedMods = GetLoadedMods();
            g_modsScanned = true;
        }
        return g_cachedMods;
    }

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

    void RunOnMainThreadAndWait(const std::function<void()>& task) {
        std::mutex m;
        std::condition_variable cv;
        bool done = false;

        RunOnMainThread([&]() {
            task();
            {
                std::lock_guard<std::mutex> lk(m);
                done = true;
            }
            cv.notify_one();
            });

        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&] { return done; });
    }

    static void __cdecl MSZ_SehTranslator(unsigned int code, EXCEPTION_POINTERS* /*p*/) {
        throw std::runtime_error("SEH exception code: " + std::to_string(code));
    }

    static void MSZ_InstallSehTranslatorOnce() {
        static bool done = false;
        if (!done) {    
            _set_se_translator(MSZ_SehTranslator);
            done = true;
        }
    }

    namespace Mods {

        struct ModLifecycle {
            ActiveMod mod;
            MSZ_OnLoad_t   onLoad = nullptr;
            MSZ_OnUpdate_t onUpdate = nullptr;
            MSZ_OnGUI_t    onGUI = nullptr;
            MSZ_OnUnload_t onUnload = nullptr;
            bool calledLoad = false;
        };

        static std::vector<ModLifecycle> g_mods;
        static std::mutex g_modsMutex;
        static bool g_inited = false;

        static void Resolve(ModLifecycle& m) {
            if (!m.mod.hModule) return;

            m.onLoad = (MSZ_OnLoad_t)GetProcAddress(m.mod.hModule, "MSZ_OnLoad");
            m.onUpdate = (MSZ_OnUpdate_t)GetProcAddress(m.mod.hModule, "MSZ_OnUpdate");
            m.onGUI = (MSZ_OnGUI_t)GetProcAddress(m.mod.hModule, "MSZ_OnGUI");
            m.onUnload = (MSZ_OnUnload_t)GetProcAddress(m.mod.hModule, "MSZ_OnUnload");

            LogI("Resolve %s: OnLoad=%p OnUpdate=%p OnGUI=%p OnUnload=%p",
                m.mod.info.Name ? m.mod.info.Name : "(unknown)",
                m.onLoad, m.onUpdate, m.onGUI, m.onUnload);
        }

        static void SafeInvoke(const char* phase, ModLifecycle* pm, void(*fn)()) {
            if (!pm || !fn) return;
            try {
                fn();
            }
            catch (const std::exception& e) {
                LogE("Mod %s threw std::exception (%s): %s",
                    phase,
                    pm->mod.info.Name ? pm->mod.info.Name : "(unknown)",
                    e.what());
            }
            catch (...) {
                LogE("Mod %s crashed (%s)",
                    phase,
                    pm->mod.info.Name ? pm->mod.info.Name : "(unknown)");
            }
        }

        void Init() {
            std::vector<ModLifecycle*> toLoad;

            {
                std::lock_guard<std::mutex> lock(g_modsMutex);
                if (g_inited) return;

                auto mods = GetLoadedModsCached(true);

                g_mods.clear();
                g_mods.reserve(mods.size());

                for (auto& am : mods) {
                    ModLifecycle ml;
                    ml.mod = am;
                    Resolve(ml);
                    g_mods.push_back(ml);
                }

                for (auto& m : g_mods) {
                    if (m.onLoad && !m.calledLoad) {
                        m.calledLoad = true;
                        toLoad.push_back(&m);
                    }
                }

                g_inited = true;
                LogI("Mods lifecycle initialized (%zu mods)", g_mods.size());
            }

            // schedule OnLoad on main thread (outside lock)
            for (auto* pm : toLoad) {
                RunOnMainThread([pm]() {
                    SafeInvoke("OnLoad", pm, pm->onLoad);
                    });
            }
        }

        void TickUpdate() {
            std::vector<ModLifecycle*> list;

            {
                std::lock_guard<std::mutex> lock(g_modsMutex);
                if (!g_inited) return;
                list.reserve(g_mods.size());
                for (auto& m : g_mods) if (m.onUpdate) list.push_back(&m);
            }

            for (auto* pm : list) {
                SafeInvoke("OnUpdate", pm, pm->onUpdate);
            }
        }

        void TickGUI() {
            std::vector<ModLifecycle*> list;

            {
                std::lock_guard<std::mutex> lock(g_modsMutex);
                if (!g_inited) return;
                list.reserve(g_mods.size());
                for (auto& m : g_mods) if (m.onGUI) list.push_back(&m);
            }

            for (auto* pm : list) {
                SafeInvoke("OnGUI", pm, pm->onGUI);
            }
        }

        void Shutdown() {
            std::vector<ModLifecycle*> toUnload;

            {
                std::lock_guard<std::mutex> lock(g_modsMutex);
                if (!g_inited) return;

                toUnload.reserve(g_mods.size());
                for (auto& m : g_mods) if (m.onUnload) toUnload.push_back(&m);

                g_inited = false;
            }

            // call unload outside lock
            for (auto* pm : toUnload) {
                SafeInvoke("OnUnload", pm, pm->onUnload);
            }

            {
                std::lock_guard<std::mutex> lock(g_modsMutex);
                g_mods.clear();
            }

            LogI("Mods lifecycle shutdown");
        }
    }

    namespace Events {
        static std::mutex g_evtMutex;
        static std::vector<Handler> g_onUpdate;
        static std::vector<Handler> g_onGUI;
        static std::vector<KeyHandler> g_onKeyDown;

        void OnUpdate(const Handler& fn) {
            std::lock_guard<std::mutex> lock(g_evtMutex);
            g_onUpdate.push_back(fn);
        }
        void OnGUI(const Handler& fn) {
            std::lock_guard<std::mutex> lock(g_evtMutex);
            g_onGUI.push_back(fn);
        }
        void OnKeyDown(const KeyHandler& fn) {
            std::lock_guard<std::mutex> lock(g_evtMutex);
            g_onKeyDown.push_back(fn);
        }

        namespace Internal {
            void Tick() {
                std::vector<Handler> copy;
                {
                    std::lock_guard<std::mutex> lock(g_evtMutex);
                    copy = g_onUpdate;
                }
                for (auto& fn : copy) if (fn) fn();
            }

            void TickGUI() {
                std::vector<Handler> copy;
                {
                    std::lock_guard<std::mutex> lock(g_evtMutex);
                    copy = g_onGUI;
                }
                for (auto& fn : copy) if (fn) fn();
            }

            void DispatchKeyDown(KeyCode key) {
                std::vector<KeyHandler> copy;
                {
                    std::lock_guard<std::mutex> lock(g_evtMutex);
                    copy = g_onKeyDown;
                }
                for (auto& fn : copy) if (fn) fn(key);
            }

            void ClearAll() {
                std::lock_guard<std::mutex> lock(g_evtMutex);
                g_onUpdate.clear();
                g_onGUI.clear();
                g_onKeyDown.clear();
            }
        }
    }

    namespace Scheduler {
        struct ScheduledTask {
            TaskId id;
            Task fn;
            float interval;
            float remaining;
            bool cancelled;
        };

        static std::mutex g_schedMutex;
        static std::vector<ScheduledTask> g_tasks;
        static std::atomic<uint64_t> g_nextId{ 1 };

        static TaskId Add(float seconds, float interval, Task fn) {
            ScheduledTask t;
            t.id = g_nextId.fetch_add(1);
            t.fn = std::move(fn);
            t.interval = interval;
            t.remaining = seconds;
            t.cancelled = false;
            std::lock_guard<std::mutex> lock(g_schedMutex);
            g_tasks.push_back(std::move(t));
            return g_tasks.back().id;
        }

        TaskId RunAfter(float seconds, Task fn) {
            if (seconds < 0.f) seconds = 0.f;
            return Add(seconds, 0.f, std::move(fn));
        }

        TaskId RunEvery(float seconds, Task fn) {
            if (seconds < 0.01f) seconds = 0.01f;
            return Add(seconds, seconds, std::move(fn));
        }

        TaskId RunNextFrame(Task fn) {
            return Add(0.f, 0.f, std::move(fn));
        }

        void Cancel(TaskId id) {
            std::lock_guard<std::mutex> lock(g_schedMutex);
            for (auto& t : g_tasks) if (t.id == id) t.cancelled = true;
        }

        namespace Internal {
            void Tick(float unscaledDeltaSeconds) {
                std::vector<Task> toRun;
                {
                    std::lock_guard<std::mutex> lock(g_schedMutex);
                    for (auto& t : g_tasks) {
                        if (t.cancelled) continue;
                        t.remaining -= unscaledDeltaSeconds;
                        if (t.remaining <= 0.f) {
                            if (t.fn) toRun.push_back(t.fn);
                            if (t.interval > 0.f) {
                                t.remaining += t.interval;
                            }
                            else {
                                t.cancelled = true;
                            }
                        }
                    }

                    g_tasks.erase(std::remove_if(g_tasks.begin(), g_tasks.end(),
                        [](const ScheduledTask& t) { return t.cancelled; }),
                        g_tasks.end());
                }
                for (auto& fn : toRun) fn();
            }

            void ClearAll() {
                std::lock_guard<std::mutex> lock(g_schedMutex);
                g_tasks.clear();
            }
        }
    }

    namespace Unity {
        void* GetMainCamera() {
            if (Hook::Unity::GetMainCamera) {
                return Hook::Unity::GetMainCamera();
            }
            return nullptr;
        }

        bool GetKey(KeyCode key) {
            if (Hook::Unity::GetKey) {
                return Hook::Unity::GetKey(key);
            }
            return false;
        }

        bool GetKeyDown(KeyCode key)
        {
            if (Hook::Unity::GetKeyDown)
                return Hook::Unity::GetKeyDown(key);

            return false;
        }

        bool GetKeyUp(KeyCode key)
        {
            if (Hook::Unity::GetKeyUp)
                return Hook::Unity::GetKeyUp(key);

            return false;
        }

        bool GetButton(const char* name)
        {
            if (Hook::Unity::GetButton)
                return Hook::Unity::GetButton(name);

            return false;
        }

        bool GetButtonDown(const char* name)
        {
            if (Hook::Unity::GetButtonDown)
                return Hook::Unity::GetButtonDown(name);

            return false;
        }

        bool GetButtonUp(const char* name)
        {
            if (Hook::Unity::GetButtonUp)
                return Hook::Unity::GetButtonUp(name);

            return false;
        }

        bool GetMouseButton(int button)
        {
            if (Hook::Unity::GetMouseButton)
                return Hook::Unity::GetMouseButton(button);

            return false;
        }

        bool GetMouseButtonDown(int button)
        {
            if (Hook::Unity::GetMouseButtonDown)
                return Hook::Unity::GetMouseButtonDown(button);

            return false;
        }

        bool GetMouseButtonUp(int button)
        {
            if (Hook::Unity::GetMouseButtonUp)
                return Hook::Unity::GetMouseButtonUp(button);

            return false;
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
            static std::unordered_map<std::string, void*> cache;
            static std::mutex cacheMutex;

            void* Get(const char* assemblyQualifiedName) {
                if (!assemblyQualifiedName) return nullptr;

                {
                    std::lock_guard<std::mutex> lock(cacheMutex);
                    auto it = cache.find(assemblyQualifiedName);
                    if (it != cache.end()) return it->second;
                }

                // Resolve without holding the lock (potentially expensive)
                void* typeObj = GetTypeObject(assemblyQualifiedName);
                if (!typeObj) return nullptr;

                std::lock_guard<std::mutex> lock(cacheMutex);
                cache[assemblyQualifiedName] = typeObj;
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
        }

        namespace Internal {
            void RenderAll() {
                if (!m_ShowMenu) return;
                std::lock_guard<std::mutex> lock(m_RenderMutex);

                MSZ_API::Events::Internal::TickGUI();

                if (MSZ_API::Initialized) {
                    MSZ_API::Mods::TickGUI();
                }

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

#pragma once

#include <windows.h>
#include <iostream>
#include <vector>
#include <queue>
#include <mutex>
#include <functional>
#include <map>
#include <string>

#ifdef MSZ_API_EXPORTS
#define MSZ_API_FUNC __declspec(dllexport)
#else
#define MSZ_API_FUNC __declspec(dllimport)
#pragma comment(lib, "MSZModdingAPIDLL.lib")
#endif

// --- Mod Metadata Structure ---
// Modders must implement this to appear in the active mod list
struct MSZ_ModInfo {
    const char* Name;
    const char* Author;
    const char* Version;
    const char* Description;
};

// --- Log Helpers ---
inline void _LogGeneric(int color, const char* prefix, const char* format, va_list args) {
    int size = vsnprintf(nullptr, 0, format, args) + 1;
    if (size <= 0) return;
    std::vector<char> buffer(size);
    vsnprintf(buffer.data(), size, format, args);
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
    std::cout << prefix << buffer.data() << std::endl;
    SetConsoleTextAttribute(hConsole, 7);
}

inline void LogI(const char* format, ...) {
    va_list args; va_start(args, format);
    _LogGeneric(10, "[MODDING-API][INFORMATION]: ", format, args);
    va_end(args);
}

inline void LogW(const char* format, ...) {
    va_list args; va_start(args, format);
    _LogGeneric(14, "[MODDING-API][WARNING]: ", format, args);
    va_end(args);
}

inline void LogE(const char* format, ...) {
    va_list args; va_start(args, format);
    _LogGeneric(12, "[MODDING-API][ERROR]: ", format, args);
    va_end(args);
}

// --- Unity Data Structures ---
struct Vector3 {
    float x, y, z;
    Vector3(float _x = 0, float _y = 0, float _z = 0) : x(_x), y(_y), z(_z) {}
    Vector3 operator+(const Vector3& other) const { return { x + other.x, y + other.y, z + other.z }; }
    Vector3 operator-(const Vector3& other) const { return { x - other.x, y - other.y, z - other.z }; }
    Vector3 operator*(float scalar) const { return { x * scalar, y * scalar, z * scalar }; }
    Vector3 operator/(float scalar) const { return { x / scalar, y / scalar, z / scalar }; }
};

struct Vector2 {
    float x, y;
    Vector2(float _x = 0, float _y = 0) : x(_x), y(_y) {}
};

struct Quaternion {
    float x, y, z, w;
    Quaternion(float _x = 0, float _y = 0, float _z = 0, float _w = 1) : x(_x), y(_y), z(_z), w(_w) {}
};

struct Ray {
    Vector3 origin;
    Vector3 direction;
};

struct RaycastHit {
    Vector3 m_Point;
    Vector3 m_Normal;
    unsigned int m_FaceID;
    float m_Distance;
    Vector2 m_UV;
    int m_ColliderID;
    int m_Padding;
    void* m_Collider;
};

struct PhysicsScene {
    int handle;
};

// --- MSZ Modding API ---
namespace MSZ_API {
    MSZ_API_FUNC void ProcessMainThreadTasks();
    MSZ_API_FUNC void RunOnMainThread(std::function<void()> task);
    MSZ_API_FUNC extern bool Initialized;

    // --- NEW: Mod Registry ---
    struct ActiveMod {
        HMODULE hModule;
        MSZ_ModInfo info;
    };

    // Allows the API to track all loaded DLLs and their info
    MSZ_API_FUNC std::vector<ActiveMod> GetLoadedMods();

    namespace UI {
        // --- Core System ---
        MSZ_API_FUNC void RegisterMenu(std::function<void()> callback);
        MSZ_API_FUNC void SetMenuOpen(bool open);
        MSZ_API_FUNC bool IsMenuOpen();
        MSZ_API_FUNC void ToggleMenu();

        namespace Internal {
            void RenderAll();
        }

        // --- Wrappers ---
        MSZ_API_FUNC bool Begin(const char* name);
        MSZ_API_FUNC void End();
        MSZ_API_FUNC void Separator();
        MSZ_API_FUNC void SameLine();
        MSZ_API_FUNC void Spacing();
        MSZ_API_FUNC void Text(const char* fmt, ...);
        MSZ_API_FUNC bool Button(const char* label);
        MSZ_API_FUNC bool Checkbox(const char* label, bool* v);
        MSZ_API_FUNC bool SliderFloat(const char* label, float* v, float v_min, float v_max);
        MSZ_API_FUNC bool InputText(const char* label, char* buf, size_t buf_size);
    }

    namespace Unity {
        namespace TypeCache {
            MSZ_API_FUNC void* Get(const char* assemblyQualifiedName);
        }

        namespace World {
            MSZ_API_FUNC void SetTimeScale(float scale);
            MSZ_API_FUNC float GetTimeScale();
            MSZ_API_FUNC void LoadScene(const char* sceneName);
        }

        namespace Physics {
            MSZ_API_FUNC void SetGravity(void* rigidbody, bool enabled);
            MSZ_API_FUNC void SetKinematic(void* rigidbody, bool enabled);
        }

        MSZ_API_FUNC void* GetMainCamera();
        MSZ_API_FUNC Vector3 GetMousePosition();
        MSZ_API_FUNC Ray ScreenPointToRay(void* camera, Vector3 position);
        MSZ_API_FUNC bool Raycast(Ray* ray, float maxDistance, RaycastHit* outHit, int layerMask = -1, int interaction = 1);
        MSZ_API_FUNC Ray CreateWorldRay(void* camera);
        MSZ_API_FUNC void* GetGameObject(void* componentOrCollider);
        MSZ_API_FUNC void Destroy(void* unityObject);
        MSZ_API_FUNC void Destroy(int instanceID);
        MSZ_API_FUNC void DestroyGameObject(void* componentOrObject);
        MSZ_API_FUNC void* FindObjectFromInstanceID(int instanceID);
        MSZ_API_FUNC void* GetTransform(void* unityObject);
        MSZ_API_FUNC Vector3 GetPosition(void* transform);
        MSZ_API_FUNC Vector3 GetPositionFromObject(void* unityObject);
        MSZ_API_FUNC Vector3 GetForward(void* unityObject);
        MSZ_API_FUNC void* GetTypeObject(const char* assemblyQualifiedName);
    }

    namespace Player {
        MSZ_API_FUNC void SetPlayerMovementSpeed(float speed);
        MSZ_API_FUNC float GetPlayerMovementSpeed();
        MSZ_API_FUNC void ToggleMovement(bool enable, bool smoothSpeed);
        MSZ_API_FUNC void* GetPlayer();
        MSZ_API_FUNC void* GetRigidbody();
        MSZ_API_FUNC void* GetTransform();
        MSZ_API_FUNC void SetVelocity(Vector3 velocity);
        MSZ_API_FUNC void Teleport(Vector3 position);

        extern MSZ_API_FUNC float originalSpeed;
        extern MSZ_API_FUNC float currentSpeed;
        extern MSZ_API_FUNC bool isSpeedModified;
    }
}
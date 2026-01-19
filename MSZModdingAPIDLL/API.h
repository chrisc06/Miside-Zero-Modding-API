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

    // Vector Addition/Subtraction
    Vector3 operator+(const Vector3& other) const { return { x + other.x, y + other.y, z + other.z }; }
    Vector3 operator-(const Vector3& other) const { return { x - other.x, y - other.y, z - other.z }; }

    // Scalar Multiplication/Division
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

struct Color { float r, g, b, a; };

struct RaycastHit {
    Vector3 m_Point;          // 0x00 - 12 bytes
    Vector3 m_Normal;         // 0x0C - 12 bytes
    unsigned int m_FaceID;    // 0x18 - 4 bytes
    float m_Distance;         // 0x1C - 4 bytes
    Vector2 m_UV;             // 0x20 - 8 bytes
    int m_ColliderID;         // 0x28 - 4 bytes (CRITICAL: Padding/ID field)
    int m_Padding;            // 0x2C - 4 bytes (Ensures the pointer starts at 0x30)
    void* m_Collider;         // 0x30 - 8 bytes (Pointer)
};

struct PhysicsScene {
    int handle;
};

// --- MSZ Modding API ---
namespace MSZ_API {
    MSZ_API_FUNC void ProcessMainThreadTasks();
    MSZ_API_FUNC void RunOnMainThread(std::function<void()> task);
    MSZ_API_FUNC extern bool Initialized;

    namespace UI {
        // --- Core System ---
        MSZ_API_FUNC void RegisterMenu(std::function<void()> callback);

        // Control the visibility of the menu system
        MSZ_API_FUNC void SetMenuOpen(bool open);
        MSZ_API_FUNC bool IsMenuOpen();
        MSZ_API_FUNC void ToggleMenu();

        // --- INTERNAL (Used by main.cpp only) ---
        namespace Internal {
            // Called by the DirectX Hook to render all registered menus
            void RenderAll();
        }

        // --- Wrappers (User calls these, NOT ImGui directly) ---

        // Windows
        MSZ_API_FUNC bool Begin(const char* name); // Returns false if collapsed
        MSZ_API_FUNC void End();

        // Layout
        MSZ_API_FUNC void Separator();
        MSZ_API_FUNC void SameLine();
        MSZ_API_FUNC void Spacing();

        // Widgets
        MSZ_API_FUNC void Text(const char* fmt, ...);
        MSZ_API_FUNC bool Button(const char* label); // Returns true if clicked
        MSZ_API_FUNC bool Checkbox(const char* label, bool* v); // Returns true if value changed
        MSZ_API_FUNC bool SliderFloat(const char* label, float* v, float v_min, float v_max); // Returns true if value changed
        MSZ_API_FUNC bool InputText(const char* label, char* buf, size_t buf_size);
    }

    namespace Unity {
        // Type resolution with caching
        namespace TypeCache {
            MSZ_API_FUNC void* Get(const char* assemblyQualifiedName);
        }

        namespace World {
            MSZ_API_FUNC void SetTimeScale(float scale);
            MSZ_API_FUNC float GetTimeScale();
            MSZ_API_FUNC void LoadScene(const char* index);
        }

        namespace Physics {
            MSZ_API_FUNC void SetGravity(void* rigidbody, bool enabled);
            MSZ_API_FUNC void SetKinematic(void* rigidbody, bool enabled);
        }

        // Camera and Input
        MSZ_API_FUNC void* GetMainCamera();
        MSZ_API_FUNC Vector3 GetMousePosition();
        MSZ_API_FUNC Ray ScreenPointToRay(void* camera, Vector3 position);

        // Physics
        MSZ_API_FUNC bool Raycast(Ray* ray, float maxDistance, RaycastHit* outHit, int layerMask = -1, int interaction = 1);
        MSZ_API_FUNC Ray CreateWorldRay(void* camera);

        // Object Management
        MSZ_API_FUNC void* GetGameObject(void* componentOrCollider);
        MSZ_API_FUNC void Destroy(void* unityObject);
        MSZ_API_FUNC void Destroy(int instanceID);
        MSZ_API_FUNC void DestroyGameObject(void* componentOrObject);
        MSZ_API_FUNC void* FindObjectFromInstanceID(int instanceID);

        // Transform Operations
        MSZ_API_FUNC void* GetTransform(void* unityObject);
        MSZ_API_FUNC Vector3 GetPosition(void* transform);
        MSZ_API_FUNC Vector3 GetPositionFromObject(void* unityObject);
        MSZ_API_FUNC Vector3 GetForward(void* unityObject);

        // Type System
        MSZ_API_FUNC void* GetTypeObject(const char* assemblyQualifiedName);
    }

    namespace Player {
        // Movement
        MSZ_API_FUNC void SetPlayerMovementSpeed(float speed);
        MSZ_API_FUNC float GetPlayerMovementSpeed();
        MSZ_API_FUNC void ToggleMovement(bool enable, bool smoothSpeed);

        // Object Access
        MSZ_API_FUNC void* GetPlayer();
        MSZ_API_FUNC void* GetRigidbody();
        MSZ_API_FUNC void* GetTransform();

        // Physics Operations
        MSZ_API_FUNC void SetVelocity(Vector3 velocity);
        MSZ_API_FUNC void Teleport(Vector3 position);

        // State Variables
        extern MSZ_API_FUNC float originalSpeed;
        extern MSZ_API_FUNC float currentSpeed;
        extern MSZ_API_FUNC bool isSpeedModified;
    }
}
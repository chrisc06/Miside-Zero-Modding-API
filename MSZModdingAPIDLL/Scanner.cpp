#include "Scanner.h"
#include "Hook.h"
#include "API.h"

void Scanner::Initialize() {
    using namespace Hook::Unity;

    // --- Original Gameplay Scans ---
    Cache["kiriMoveBasic::Update"] = GetMethod("Assembly-CSharp", "", "kiriMoveBasic", "Update", 0);
    Cache["kiriMoveBasic::ToggleMovement"] = GetMethod("Assembly-CSharp", "", "kiriMoveBasic", "ToggleMovement", 2);
    Cache["kiriMoveBasic::walkSpeed"] = GetField("Assembly-CSharp", "", "kiriMoveBasic", "walkSpeed");
    uintptr_t loadSceneAddr = GetMethod("UnityEngine.SceneManagementModule", "UnityEngine.SceneManagement", "SceneManager", "LoadScene", 1);

    if (loadSceneAddr) {
        Hook::Unity::LoadScene = (Hook::Unity::LoadScene_t)loadSceneAddr;
        LogI("Scanner: Resolved SceneManager::LoadScene at 0x%p", loadSceneAddr);
    }
    LogI("Scanner: Successfully mapped %llu dynamic targets.", Cache.size());
}

uintptr_t Scanner::GetMethod(const char* asmName, const char* ns, const char* clss, const char* meth, int args) {
    void* domain = Hook::Unity::domain_get();
    void* assembly = Hook::Unity::assembly_open(domain, asmName);
    if (!assembly) return 0;

    void* image = Hook::Unity::assembly_get_image(assembly);
    void* klass = Hook::Unity::class_from_name(image, ns, clss);
    if (!klass) return 0;

    void* method = Hook::Unity::class_get_method_from_name(klass, meth, args);
    if (!method) {
        LogE("Scanner: Could not find method %s in class %s", meth, clss);
        return 0;
    }

    // In IL2CPP, the first member of MethodInfo is the methodPointer.
    uintptr_t fnPtr = *(uintptr_t*)method;
    if (!fnPtr) {
        LogW("Scanner: Method %s found, but methodPointer is NULL (Abstract/Internal?)", meth);
    }
    return fnPtr;
}

uintptr_t Scanner::GetField(const char* asmName, const char* ns, const char* clss, const char* fieldName) {
    void* domain = Hook::Unity::domain_get();
    void* assembly = Hook::Unity::assembly_open(domain, asmName);
    if (!assembly) return 0;
    void* image = Hook::Unity::assembly_get_image(assembly);
    void* klass = Hook::Unity::class_from_name(image, ns, clss);
    if (!klass) return 0;

    void* field = Hook::Unity::class_get_field_from_name(klass, fieldName);
    if (!field) return 0;

    return (uintptr_t)Hook::Unity::field_get_offset(field);
}

void* Scanner::GetClass(const char* assemblyName, const char* namespaze, const char* className) {
    void* domain = Hook::Unity::domain_get();
    void* assembly = Hook::Unity::assembly_open(domain, assemblyName);
    if (!assembly) return nullptr;

    void* image = Hook::Unity::assembly_get_image(assembly);
    return Hook::Unity::class_from_name(image, namespaze, className);
}
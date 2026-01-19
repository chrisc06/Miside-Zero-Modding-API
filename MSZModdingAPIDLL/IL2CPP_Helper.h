#pragma once
#include <windows.h>

typedef void* (*il2cpp_domain_get_t)();
typedef void* (*il2cpp_domain_assembly_open_t)(void* domain, const char* name);
typedef void* (*il2cpp_assembly_get_image_t)(void* assembly);
typedef void* (*il2cpp_class_from_name_t)(void* image, const char* namespaze, const char* name);
typedef void* (*il2cpp_class_get_method_from_name_t)(void* klass, const char* name, int argsCount);
typedef void* (*il2cpp_runtime_invoke_t)(void* method, void* obj, void** params, void** exc);
typedef void* (*il2cpp_thread_attach_t)(void* domain);

namespace IL2CPP {
    inline il2cpp_domain_get_t domain_get;
    inline il2cpp_domain_assembly_open_t assembly_open;
    inline il2cpp_assembly_get_image_t assembly_get_image;
    inline il2cpp_class_from_name_t class_from_name;
    inline il2cpp_class_get_method_from_name_t class_get_method_from_name;
    inline il2cpp_runtime_invoke_t runtime_invoke;
    inline il2cpp_thread_attach_t thread_attach;

    inline void Init() {
        HMODULE gameAssembly = GetModuleHandleA("GameAssembly.dll");
        if (!gameAssembly) return;

        domain_get = (il2cpp_domain_get_t)GetProcAddress(gameAssembly, "il2cpp_domain_get");
        assembly_open = (il2cpp_domain_assembly_open_t)GetProcAddress(gameAssembly, "il2cpp_domain_assembly_open");
        assembly_get_image = (il2cpp_assembly_get_image_t)GetProcAddress(gameAssembly, "il2cpp_assembly_get_image");
        class_from_name = (il2cpp_class_from_name_t)GetProcAddress(gameAssembly, "il2cpp_class_from_name");
        class_get_method_from_name = (il2cpp_class_get_method_from_name_t)GetProcAddress(gameAssembly, "il2cpp_class_get_method_from_name");
        runtime_invoke = (il2cpp_runtime_invoke_t)GetProcAddress(gameAssembly, "il2cpp_runtime_invoke");

        // Resolve thread_attach
        thread_attach = (il2cpp_thread_attach_t)GetProcAddress(gameAssembly, "il2cpp_thread_attach");
    }
} 
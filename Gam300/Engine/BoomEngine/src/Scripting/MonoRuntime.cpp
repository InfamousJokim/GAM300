#include "Core.h"
#include "Scripting/MonoRuntime.h"
#include "Scripting/ScriptBinding.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace Boom {

    // Define static members
    MonoDomain* MonoRuntime::m_RootDomain = nullptr;
    MonoDomain* MonoRuntime::m_AppDomain = nullptr;

    bool MonoRuntime::Init(const char* domainName, const char* assembliesPath)
    {
        if (assembliesPath && *assembliesPath)
            mono_set_assemblies_path(assembliesPath);

#ifdef _WIN32
        char exePath[MAX_PATH];
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();

        // Detect shipped mode vs development mode
        // Shipped mode: etc/ and lib/ folders are next to the exe
        // Development mode: mono/ folder is at repo root (../../../mono/)
        std::filesystem::path monoLib;
        std::filesystem::path monoEtc;

        bool isShippedMode = std::filesystem::exists(exeDir / "etc") &&
                            std::filesystem::exists(exeDir / "lib");

        if (isShippedMode)
        {
            // SHIPPED MODE: Mono folders are next to exe
            monoLib = exeDir / "lib";
            monoEtc = exeDir / "etc";
            BOOM_INFO("[Mono] Running in SHIPPED mode (mono runtime next to exe)");
        }
        else
        {
            // DEVELOPMENT MODE: Mono is at repo root
            std::filesystem::path repoRoot = exeDir.parent_path().parent_path().parent_path();
            std::filesystem::path monoRoot = repoRoot / "mono";
            monoLib = monoRoot / "lib";
            monoEtc = monoRoot / "etc";
            BOOM_INFO("[Mono] Running in DEVELOPMENT mode (mono at repo root)");
        }

        std::string libPath = std::filesystem::absolute(monoLib).string();
        std::string etcPath = std::filesystem::absolute(monoEtc).string();

        std::replace(libPath.begin(), libPath.end(), '\\', '/');
        std::replace(etcPath.begin(), etcPath.end(), '\\', '/');

#ifdef DEBUG
        BOOM_INFO("[Mono] libPath = {}", libPath);
        BOOM_INFO("[Mono] etcPath = {}", etcPath);
#endif

        mono_set_dirs(libPath.c_str(), etcPath.c_str());
#else
        mono_set_dirs("/usr/lib", "/etc/mono");
#endif

        BOOM_INFO("[Mono] Initializing Mono JIT...");

        // Create root domain only once for the whole process.
        if (!m_RootDomain)
        {
            m_RootDomain = mono_jit_init(domainName ? domainName : "BoomDomain");
            if (!m_RootDomain) {
#ifdef DEBUG
                BOOM_ERROR("Mono: mono_jit_init failed - check Mono installation paths");
#endif
                return false;
            }
        }
        else
        {
            // Make sure we are back on root before messing with app domains
            mono_domain_set(m_RootDomain, /*force*/ false);
        }

        // If we had an app domain from a previous run, unload it first
        if (m_AppDomain)
        {
            mono_domain_unload(m_AppDomain);
            m_AppDomain = nullptr;
        }

        // Fresh app domain for your scripts
        m_AppDomain = mono_domain_create_appdomain(const_cast<char*>("BoomApp"), nullptr);
        if (!m_AppDomain) {
#ifdef DEBUG
            BOOM_ERROR("Mono: failed to create app domain");
#endif
            return false;
        }
        mono_domain_set(m_AppDomain, /*force*/ false);

#ifdef DEBUG
        BOOM_INFO("[Mono] Initialized: {}", RuntimeInfo());
#endif

        m_LoadedAssemblies.clear();
        return true;
    }

    void MonoRuntime::UnloadDomain()
    {
        // 1) CRITICAL: Clear all trigger callbacks FIRST before domain unload
        //    This prevents access violations when callbacks try to call into destroyed Mono domain
        ClearAllTriggerCallbacks();

        // 2) Invalidate all assembly handles immediately
        m_LoadedAssemblies.clear();

        // 3) Then unload the app domain (if any)
        if (m_AppDomain) {
            mono_domain_set(m_RootDomain, /*force*/ false);
            mono_domain_unload(m_AppDomain);
            m_AppDomain = nullptr;
        }
    }

    void MonoRuntime::Shutdown()
    {

        m_LoadedAssemblies.clear();
        UnloadDomain();
        if (m_RootDomain) {
            mono_jit_cleanup(m_RootDomain);
            m_RootDomain = nullptr;
        }
#ifdef DEBUG
        BOOM_INFO("[Mono] Shutdown complete");
#endif
    }

    MonoAssembly* MonoRuntime::LoadAssembly(const char* path)
    {
        if (!path) return nullptr;

        if (m_AppDomain)
            mono_domain_set(m_AppDomain, /*force*/ false);

        MonoAssembly* asmHandle = mono_domain_assembly_open(mono_domain_get(), path);
        if (!asmHandle) {
#ifdef DEBUG
            BOOM_ERROR("[Mono] Failed to load assembly: {}", path);
#endif
            return nullptr;
        }
#ifdef DEBUG
        MonoImage* img = mono_assembly_get_image(asmHandle);
        BOOM_INFO("[Mono] Loaded assembly: {} (image ok={})",
            path, img ? "true" : "false");
#endif
        m_LoadedAssemblies.push_back(asmHandle);
        return asmHandle;
    }

    bool MonoRuntime::InvokeStatic(const char* fullMethodDesc, void** args, int argCount)
    {
        (void)argCount;
        if (!fullMethodDesc) return false;

        // If runtime / app domain is gone, do NOT call into Mono
        if (!m_RootDomain || !m_AppDomain) {
#ifdef DEBUG
            BOOM_WARN("[Mono] InvokeStatic called after shutdown/unload ({}). Ignoring.", fullMethodDesc);
#endif
            return false;
        }

        mono_domain_set(m_AppDomain, /*force*/ false);

        MonoMethodDesc* desc = mono_method_desc_new(fullMethodDesc, /*include_namespace*/ true);
        if (!desc) {
#ifdef DEBUG
            BOOM_ERROR("[Mono] Bad method desc: {}", fullMethodDesc);
#endif
            return false;
        }

        MonoMethod* method = nullptr;

        if (MonoImage* imgCorlib = mono_get_corlib())
            method = mono_method_desc_search_in_image(desc, imgCorlib);

        if (!method) {
            for (MonoAssembly* a : m_LoadedAssemblies) {
                if (!a) continue;
                MonoImage* img = mono_assembly_get_image(a);
                if (!img) continue;       // *** extra safety ***
                method = mono_method_desc_search_in_image(desc, img);
                if (method) break;
            }
        }

        mono_method_desc_free(desc);
        if (!method) {
#ifdef DEBUG
            BOOM_ERROR("[Mono] Method not found: {}", fullMethodDesc);
#endif
            return false;
        }

        MonoObject* exc = nullptr;
        mono_runtime_invoke(method, nullptr, args, &exc);
        if (exc) {
            LogException(exc, "[Mono] Exception");
            return false;
        }
#ifdef DEBUG
        BOOM_INFO("[Mono] Invoked: {}", fullMethodDesc);
#endif
        return true;
    }


    // ===== Helpers for instance creation / reflection =====

    // Accepts "Namespace.Type" (no assembly name).
    MonoClass* MonoRuntime::FindClassByName(const char* fullName) const
    {
        if (!fullName) return nullptr;

        // Split into namespace + name
        const char* dot = strrchr(fullName, '.');
        std::string ns, name;
        if (dot) { ns.assign(fullName, dot - fullName); name.assign(dot + 1); }
        else { ns = ""; name = fullName; }

        // Search each loaded assembly image
        for (MonoAssembly* a : m_LoadedAssemblies) {
            if (!a) continue;
            if (MonoImage* img = mono_assembly_get_image(a)) {
                if (MonoClass* k = mono_class_from_name(img, ns.c_str(), name.c_str()))
                    return k;
            }
        }
        return nullptr;
    }

    MonoMethod* MonoRuntime::FindMethod(MonoClass* klass, const char* name, int argc) const
    {
        if (!klass || !name) return nullptr;
        // mono_class_get_method_from_name is straightforward if you know argc
        return mono_class_get_method_from_name(klass, name, argc);
    }

    MonoClassField* MonoRuntime::FindField(MonoClass* klass, const char* name) const
    {
        if (!klass || !name) return nullptr;
        return mono_class_get_field_from_name(klass, name);
    }

    void MonoRuntime::LogException(MonoObject* exc, const char* prefix) const
    {
        if (!exc) return;
        MonoString* s = mono_object_to_string(exc, nullptr);
        char* c = mono_string_to_utf8(s);
        BOOM_ERROR("{}: {}", prefix ? prefix : "[Mono] Exception", c ? c : "(null)");
        if (c) mono_free(c);
    }

    const char* MonoRuntime::RuntimeInfo() const
    {   
        return mono_get_runtime_build_info();
    }

} // namespace Boom

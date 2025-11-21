#pragma once
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "Scripting/MonoRuntime.h"
#include "Scripting/ScriptBinding.h"
#include "ECS/ECS.hpp"
#include "FileWatcher.h"

namespace Boom {
    // Forward declarations only - no includes that might cause circular deps
    struct AppContext;
    struct ScriptComponent;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4251)
#pragma warning(disable:4275)
#endif

    class BOOM_API ScriptingSystem {
    public:
        // call from Editor on startup with the folder that contains GameScripts.dll
        bool Init(const std::string& scriptsDir, AppContext* ctx);
        void Shutdown();

        bool LoadScriptsDll(const std::string& dllPath);
        bool CallStart();               // calls GameScripts.Entry:Start()
        bool CallUpdate(float dt);      // calls GameScripts.Entry:Update(float)

        // ---- Runtime instance metadata for a single C# object ----
        struct Instance {
            uint64_t    gchandle = 0;
            MonoMethod* onStart = nullptr;
            MonoMethod* onUpdate = nullptr;
            MonoMethod* onDestroy = nullptr;
        };

        // ---- Per-entity lifecycle (used by Inspector) ----
        bool RecreateForEntity(entt::entity e, ScriptComponent& sc);
        void DestroyForEntity(entt::entity e, ScriptComponent& sc);
        bool TickEntity(entt::entity e, ScriptComponent& sc, float dt);

        // ---- Whole-DLL hot reload (Inspector calls this) ----
        bool ReloadScripts();
        bool IsAlive() const { return m_Alive; }
        bool IsReloading() const { return m_Reloading; }

        // Hot reload control
        void EnableAutoHotReload(bool enable);
        bool IsAutoHotReloadEnabled() const { return m_AutoHotReload; }
        void UpdateFileWatcher();

    private:
        MonoRuntime    m_Mono;
        MonoAssembly* m_Scripts = nullptr;
        std::string    m_ScriptsDir;
        AppContext* m_Ctx = nullptr;  // Remove Boom:: prefix - already in namespace
        bool           m_Alive = false;
        bool           m_Reloading = false;  
        FileWatcher m_FileWatcher;
        bool m_AutoHotReload = true;
        std::string m_DllPath;


        bool CreateInstance(const std::string& typeName,
            const nlohmann::json& params,
            uint64_t entityHandle,
            Instance& out);

        std::unordered_map<uint64_t, Instance> m_Instances;
        uint64_t    m_NextId = 1;      // to mint InstanceId values
    };

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace Boom
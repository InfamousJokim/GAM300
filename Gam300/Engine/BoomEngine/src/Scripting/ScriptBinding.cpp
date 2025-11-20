#include "Core.h"
#include "Scripting/ScriptBinding.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>

#include "ECS/ECS.hpp"
#include "Application/Interface.h"
#include "Auxiliaries/Assets.h"
#include <glm/vec3.hpp>
#include <GLFW/glfw3.h>

#include "AppWindow.h"
#include "Input/InputHandler.h"


namespace Boom {

    static AppContext* s_Ctx = nullptr;

    static entt::entity FindEntityByName(const std::string& name) {
        auto& reg = s_Ctx->scene;
        auto view = reg.view<InfoComponent>();
        for (auto e : view) {
            if (view.get<InfoComponent>(e).name == name) return e;
        }
        return entt::null;
    }

    static void ICALL_API_Log(MonoString* msg) {
        if (!msg) return;
        char* s = mono_string_to_utf8(msg);
        if (s) { BOOM_INFO("[C#] {}", s); mono_free(s); }
    }

    static uint64_t ICALL_API_FindEntity(MonoString* name) {
        if (!name) return 0ull;
        char* s = mono_string_to_utf8(name);
        if (!s) return 0ull;
        entt::entity e = FindEntityByName(s);
        mono_free(s);
        if (e == entt::null) return 0ull;
        return static_cast<uint64_t>(static_cast<uint32_t>(e));
    }

    // NEW: Check if entity has TransformComponent
    static bool ICALL_API_HasTransform(uint64_t handle) {
        if (!s_Ctx) return false;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null) return false;
        return s_Ctx->scene.any_of<TransformComponent>(e);
    }

    // NEW: Check if entity has ScriptComponent
    static bool ICALL_API_HasScript(uint64_t handle) {
        if (!s_Ctx) return false;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null) return false;
        return s_Ctx->scene.any_of<ScriptComponent>(e);
    }

    static glm::vec3* ICALL_API_GetPosition(uint64_t handle, glm::vec3* outPos) {
        if (!s_Ctx) return nullptr;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));

        // Validate entity and component existence
        if (e == entt::null) {
            BOOM_WARN("[ScriptBinding] GetPosition: Invalid entity handle");
            return nullptr;
        }

        if (!s_Ctx->scene.valid(e)) {
            BOOM_WARN("[ScriptBinding] GetPosition: Entity no longer valid");
            return nullptr;
        }

        if (!s_Ctx->scene.any_of<TransformComponent>(e)) {
            BOOM_WARN("[ScriptBinding] GetPosition: Entity {} has no TransformComponent", static_cast<uint32_t>(e));
            return nullptr;
        }

        auto& t = s_Ctx->scene.get<TransformComponent>(e).transform;
        if (outPos) *outPos = t.translate;
        return outPos;
    }

    static void ICALL_API_SetPosition(uint64_t handle, glm::vec3* pos) {
        if (!pos || !s_Ctx) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));

        // Validate entity and component existence
        if (e == entt::null) {
            BOOM_WARN("[ScriptBinding] SetPosition: Invalid entity handle");
            return;
        }

        if (!s_Ctx->scene.valid(e)) {
            BOOM_WARN("[ScriptBinding] SetPosition: Entity no longer valid");
            return;
        }

        if (!s_Ctx->scene.any_of<TransformComponent>(e)) {
            BOOM_WARN("[ScriptBinding] SetPosition: Entity {} has no TransformComponent", static_cast<uint32_t>(e));
            return;
        }

        auto& t = s_Ctx->scene.get<TransformComponent>(e).transform;
        t.translate = *pos;
    }

    static bool ICALL_API_IsKeyDown(int key)
    {
        if (!s_Ctx || !s_Ctx->window) return false;
        return s_Ctx->window->input.keyDown(key);
    }

    static bool ICALL_API_IsMouseDown(int button) {
        if (!s_Ctx || !s_Ctx->window) return false;
        return s_Ctx->window->input.mouseDown(button);
    }



    void RegisterScriptInternalCalls(AppContext* ctx)
    {
        s_Ctx = ctx;

        // IMPORTANT: These namespaces MUST match the C# side (Boom.Native)
        mono_add_internal_call("Boom.Native::Boom_API_Log", (const void*)ICALL_API_Log);
        mono_add_internal_call("Boom.Native::Boom_API_FindEntity", (const void*)ICALL_API_FindEntity);
        mono_add_internal_call("Boom.Native::Boom_API_GetPosition", (const void*)ICALL_API_GetPosition);
        mono_add_internal_call("Boom.Native::Boom_API_SetPosition", (const void*)ICALL_API_SetPosition);
        mono_add_internal_call("Boom.Native::Boom_API_IsKeyDown", (const void*)ICALL_API_IsKeyDown);
        mono_add_internal_call("Boom.Native::Boom_API_IsMouseDown", (const void*)ICALL_API_IsMouseDown);

        // Component checking functions
        mono_add_internal_call("Boom.Native::Boom_API_HasTransform", (const void*)ICALL_API_HasTransform);
        mono_add_internal_call("Boom.Native::Boom_API_HasScript", (const void*)ICALL_API_HasScript);
    }
}
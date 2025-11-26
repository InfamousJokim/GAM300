#pragma once
#include <entt/entity/entity.hpp>
#include "Vendors/imgui/imgui.h"
#include "glm/glm.hpp"
#include "ECS/ECS.hpp"

namespace Boom { struct AppContext; struct AppInterface; }

namespace EditorUI {

    class Editor;

    class HierarchyPanel {
    public:
        explicit HierarchyPanel(Editor* owner);

        void Render();

        // Optional wiring from Editor
        void SetShowFlag(bool* flag) { m_ShowHierarchy = flag; }

        void RenderEntityNode(entt::registry& registry, entt::entity entity, Boom::AppInterface* app, Boom::AppContext* ctx,
            bool& showDeletePopup, entt::entity& entityToDelete);

    private:
        Editor* m_Owner = nullptr;
        Boom::AppInterface* m_App = nullptr;   // preferred
        Boom::AppContext* m_Ctx = nullptr;   // cached from m_App->GetContext()

        bool* m_ShowHierarchy = nullptr;

        // Delete confirmation
        bool m_ShowDeletePopup = false;
        entt::entity m_EntityToDelete = entt::null;

        //camera
        bool isTransitionCam{};
        float curTime{};
    };

} // namespace EditorUI

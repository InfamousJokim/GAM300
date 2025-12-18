#pragma once
#include <cstdint>
#include <memory>
#include "Vendors/imgui/imgui.h"
#include "Vendors/imGuizmo/ImGuizmo.h"
#include <glm/glm.hpp>
#include "Graphics/Utilities/Data.h"

namespace Boom {
    struct AppContext;
    struct AppInterface;
    class RayCast;
}

namespace EditorUI {
    class Editor;

    class SceneViewportPanel {
    public:
        explicit SceneViewportPanel(Editor* owner);
        ~SceneViewportPanel();

        void Render();
        void Show(bool v) { m_ShowViewport = v; }
        bool IsVisible() const { return m_ShowViewport; }

    private:
        std::uint32_t QuerySceneFrame() const;
        void HandleMouseClick(uint32_t enttID);
        void DrawGuizmo3D(ImVec2 const& itemMin, ImVec2 const& rectSz,
            glm::mat4 const& view, glm::mat4 const& proj, bool& gizmoWantsInput);

        Editor* m_Owner = nullptr;
        Boom::AppInterface* m_App = nullptr;
        Boom::AppContext* m_Ctx = nullptr;
        bool m_ShowViewport = true;

        // Gizmo state
        int m_GizmoOperation = ImGuizmo::TRANSLATE;
        int m_GizmoMode = ImGuizmo::WORLD;
        bool m_UseSnap = false;
        float m_SnapValues[3] = { 1.0f, 15.0f, 0.5f };
        bool m_GizmoWasUsing = false;
        Boom::Transform3D m_TransformBeforeGizmo;

        std::unique_ptr<Boom::RayCast> m_RayCast;
    };
}
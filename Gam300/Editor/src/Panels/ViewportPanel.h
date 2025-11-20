#pragma once
#include <cstdint>                 // for uint32_t
#include <memory>                  // for unique_ptr
#include "Vendors/imgui/imgui.h"   // for ImTextureID, ImVec2
#include "Vendors/imGuizmo/ImGuizmo.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/glm.hpp>

namespace Boom { struct AppContext; }
namespace Boom { struct AppInterface; }

namespace EditorUI {
    class Editor;
    class RayCast; // Forward declaration

    class ViewportPanel {
    public:
        explicit ViewportPanel(Editor* owner);
        ~ViewportPanel() = default;

        void Render();
        void OnShow();
        void OnSelect(std::uint32_t entity_id);
        void DebugViewportState() const;

        void Show(bool v) { m_ShowViewport = v; }
        bool IsVisible() const { return m_ShowViewport; }
        ImVec2 GetSize() const { return m_Viewport; }

        // Fullscreen controls
        void SetFullscreen(bool fullscreen) { m_IsFullscreen = fullscreen; }
        bool IsFullscreen() const { return m_IsFullscreen; }
        void ToggleFullscreen() { m_IsFullscreen = !m_IsFullscreen; }

    private:
        // helpers
        std::uint32_t QuerySceneFrame() const;   // prefers AppInterface::GetSceneFrame()
        double        QueryDeltaTime() const;    // prefers AppInterface::GetDeltaTime()

        // Ray casting
        void HandleMouseClick(const ImVec2& mousePos, const ImVec2& viewportSize);

    private:
        Editor* m_Owner = nullptr;  // non-owning
        Boom::AppInterface* m_App = nullptr;  // optionally sourced from owner
        Boom::AppContext* m_Ctx = nullptr;  // optionally cached via owner

        bool         m_ShowViewport = true;
        bool         m_IsFullscreen = false;

        ImTextureID  m_Frame = (ImTextureID)0;  // set in ctor
        std::uint32_t m_FrameId = 0;
        ImVec2       m_Viewport{ 0.0f, 0.0f };

        // Ray casting members
        std::unique_ptr<RayCast> m_RayCast;
        glm::mat4 m_CurrentViewMatrix{ 1.0f };
        glm::mat4 m_CurrentProjectionMatrix{ 1.0f };
        glm::vec2 m_CurrentViewportSize{ 0.0f };
        glm::vec3 m_CurrentCameraPosition{ 0.0f };

        // Gizmo state
        int m_GizmoOperation = ImGuizmo::TRANSLATE;
        int m_GizmoMode = ImGuizmo::WORLD;
        bool m_UseSnap = false;
        float m_SnapValues[3] = { 1.0f, 15.0f, 0.5f };
    };

} // namespace EditorUI
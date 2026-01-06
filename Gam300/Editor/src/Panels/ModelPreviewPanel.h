#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include "Vendors/imgui/imgui.h"
#include <glm/glm.hpp>
#include <GL/glew.h>

namespace Boom {
    struct AppContext;
    struct AppInterface;
    struct Model;
    struct Animator;
}

namespace EditorUI {
    class Editor;

    /**
     * @brief Model Preview Window - Unity-style isolated model viewer
     *
     * Features:
     * - Separate 3D viewport (independent from game scene)
     * - Orbit camera controls
     * - Load and display models with/without skeletons
     * - Skeleton visualization
     * - Foundation for animation timeline and rigging tools
     */
    class ModelPreviewPanel {
    public:
        explicit ModelPreviewPanel(Editor* owner);
        ~ModelPreviewPanel();

        void Render();

        // Model loading
        void LoadModel(const std::string& modelPath);
        void ClearModel();

    private:
        void RenderToolbar();
        void RenderViewport();
        void RenderSidebar();

        void UpdateCamera();
        void HandleCameraControls();
        void RenderModel();
        void RenderSkeleton();
        void RenderGrid();

        // Camera orbit controls
        void ResetCamera();
        void FrameModel(); // Auto-adjust camera to fit model in view

    private:
        Editor* m_Owner = nullptr;
        Boom::AppInterface* m_App = nullptr;
        Boom::AppContext* m_Ctx = nullptr;

        // Loaded model
        std::string m_LoadedModelPath;
        std::shared_ptr<Boom::Model> m_Model;
        std::shared_ptr<Boom::Animator> m_Animator;
        bool m_HasModel = false;

        // Viewport framebuffer
        GLuint m_FramebufferID = 0;
        GLuint m_TextureID = 0;
        GLuint m_DepthBufferID = 0;
        ImVec2 m_ViewportSize = { 800.0f, 600.0f };

        // Orbit camera
        glm::vec3 m_CameraPosition = glm::vec3(0.0f, 1.5f, 3.0f);
        glm::vec3 m_CameraTarget = glm::vec3(0.0f, 1.0f, 0.0f);
        float m_CameraDistance = 3.0f;
        float m_CameraYaw = 0.0f;      // Horizontal rotation
        float m_CameraPitch = 0.0f;    // Vertical rotation
        bool m_IsOrbitingCamera = false;
        ImVec2 m_LastMousePos = { 0.0f, 0.0f };

        // Visualization settings
        bool m_ShowSkeleton = true;
        bool m_ShowGrid = true;
        bool m_ShowFloor = true;
        bool m_ShowWireframe = false;
        float m_ModelScale = 1.0f;  // Preview scale adjustment

        // Selected bone (for rigging later)
        std::string m_SelectedBoneName;
    };

} // namespace EditorUI

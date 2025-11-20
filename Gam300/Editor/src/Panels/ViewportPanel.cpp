// ViewportPanel.cpp - WITH RAY CASTING
#include "Panels/ViewportPanel.h"
#include "Editor.h"
#include "Context/Context.h"
#include "Context/DebugHelpers.h"
#include "Vendors/imgui/imgui.h"
#include "RayCast.h"
#include <type_traits>
#include <cstdint>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#ifndef ICON_FA_IMAGE
#define ICON_FA_IMAGE ""
#endif

namespace {
    void DecomposeTransform(const glm::mat4& matrix, glm::vec3& position, glm::vec3& rotation, glm::vec3& scale)
    {
        position = glm::vec3(matrix[3]);
        glm::vec3 col0(matrix[0]);
        glm::vec3 col1(matrix[1]);
        glm::vec3 col2(matrix[2]);

        scale.x = glm::length(col0);
        scale.y = glm::length(col1);
        scale.z = glm::length(col2);

        if (scale.x != 0) col0 /= scale.x;
        if (scale.y != 0) col1 /= scale.y;
        if (scale.z != 0) col2 /= scale.z;

        glm::mat3 rotationMatrix(col0, col1, col2);
        rotation.y = asin(-rotationMatrix[0][2]);

        if (cos(rotation.y) != 0) {
            rotation.x = atan2(rotationMatrix[1][2], rotationMatrix[2][2]);
            rotation.z = atan2(rotationMatrix[0][1], rotationMatrix[0][0]);
        }
        else {
            rotation.x = atan2(-rotationMatrix[2][1], rotationMatrix[1][1]);
            rotation.z = 0;
        }
    }
}

namespace EditorUI {

    ViewportPanel::ViewportPanel(Editor* owner)
        : m_Owner(owner)
        , m_IsFullscreen(false)
    {
        m_App = static_cast<Boom::AppInterface*>(m_Owner);
        m_Ctx = m_App ? owner->GetContext() : nullptr;

        if (m_Ctx) {
            m_RayCast = std::make_unique<RayCast>(m_Ctx);
        }
    }

    void ViewportPanel::Render() { OnShow(); }

    void ViewportPanel::OnShow()
    {
        if (!m_ShowViewport) return;

        if (ImGui::Begin(ICON_FA_IMAGE "\tViewport", &m_ShowViewport))
        {
            ImVec2 viewportSize = ImGui::GetContentRegionAvail();
            m_Viewport = viewportSize;

            const uint32_t frameTexture = QuerySceneFrame();

            if (frameTexture > 0 && viewportSize.x > 1.0f && viewportSize.y > 1.0f)
            {
                ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddImage(
                    (ImTextureID)(uintptr_t)frameTexture,
                    cursorPos,
                    ImVec2(cursorPos.x + viewportSize.x, cursorPos.y + viewportSize.y),
                    ImVec2(0, 1),
                    ImVec2(1, 0)
                );

                ImGui::Dummy(viewportSize);

                // Get viewport bounds FIRST
                const ImVec2 itemMin = ImGui::GetItemRectMin();
                const ImVec2 itemMax = ImGui::GetItemRectMax();
                const ImVec2 rectSz = ImVec2(itemMax.x - itemMin.x, itemMax.y - itemMin.y);
                const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

                // Get gizmo state BEFORE handling mouse clicks
                bool gizmoWantsInput = false;
                if (m_Ctx)
                {
                    auto camView = m_Ctx->scene.view<Boom::CameraComponent, Boom::TransformComponent>();
                    if (camView.begin() != camView.end())
                    {
                        auto eid = *camView.begin();
                        auto& camComp = camView.get<Boom::CameraComponent>(eid);
                        auto& trans = camView.get<Boom::TransformComponent>(eid);

                        glm::mat4 view = camComp.camera.View(trans.transform);
                        const glm::mat4 proj = camComp.camera.Projection(m_Ctx->renderer->AspectRatio());

                        // Store camera data for ray casting
                        m_CurrentViewMatrix = view;
                        m_CurrentProjectionMatrix = proj;
                        m_CurrentViewportSize = glm::vec2(viewportSize.x, viewportSize.y);
                        m_CurrentCameraPosition = trans.transform.translate;

                        entt::entity selectedEntity = m_App->SelectedEntity();
                        if (selectedEntity != entt::null && m_Ctx->scene.valid(selectedEntity))
                        {
                            if (m_Ctx->scene.all_of<Boom::TransformComponent>(selectedEntity))
                            {
                                if (m_Ctx->scene.all_of<Boom::SpriteComponent>(selectedEntity) &&
                                    m_Ctx->scene.get<Boom::SpriteComponent>(selectedEntity).uiOverlay)
                                    DrawGuizmo2D(itemMin, rectSz, gizmoWantsInput);
                                else DrawGuizmo3D(itemMin, rectSz, view, proj, gizmoWantsInput);
                            }
                        }
                    }
                }

                // Handle mouse clicks for entity selection - ONLY if gizmo is not being used
                if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !gizmoWantsInput) {
                    ImVec2 mousePos = ImGui::GetMousePos();
                    ImVec2 windowPos = ImGui::GetWindowPos();
                    ImVec2 contentRegion = ImGui::GetWindowContentRegionMin();

                    ImVec2 relativeMousePos(
                        mousePos.x - windowPos.x - contentRegion.x,
                        mousePos.y - windowPos.y - contentRegion.y
                    );

                    if (relativeMousePos.x >= 0 && relativeMousePos.y >= 0 &&
                        relativeMousePos.x < viewportSize.x && relativeMousePos.y < viewportSize.y) {
                        HandleMouseClick(relativeMousePos, viewportSize);
                    }
                }

                const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && hovered;

                const ImVec2 mainPos = ImGui::GetMainViewport()->Pos;
                const double localX = double(itemMin.x - mainPos.x);
                const double localY = double(itemMin.y - mainPos.y);
                const double localW = double(rectSz.x);
                const double localH = double(rectSz.y);

                if (m_Ctx && m_Ctx->window)
                {
                    const bool allowCameraInput = hovered && focused && !gizmoWantsInput;
                    m_Ctx->window->SetCameraInputRegion(localX, localY, localW, localH, allowCameraInput);
                    m_Ctx->window->SetViewportKeyboardFocus(focused && !gizmoWantsInput);
                }

                if (hovered) ImGui::SetTooltip("Engine Viewport - Scene render output");
            }
            else {
                // Fallback UI when no frame is available
                ImGui::Text("Frame Texture ID: %u", frameTexture);
                ImGui::Text("Viewport Size: %.0fx%.0f", viewportSize.x, viewportSize.y);
                ImGui::Text("Waiting for engine frame data...");

                if (viewportSize.x > 50 && viewportSize.y > 50) {
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
                    drawList->AddRectFilled(
                        canvasPos,
                        ImVec2(canvasPos.x + viewportSize.x, canvasPos.y + viewportSize.y),
                        IM_COL32(64, 64, 64, 255)
                    );
                    drawList->AddText(
                        ImVec2(canvasPos.x + 10, canvasPos.y + 10),
                        IM_COL32(255, 255, 255, 255),
                        "Engine Viewport"
                    );
                }
            }
            
        }
        ImGui::End();
    }

    void ViewportPanel::DrawGuizmo2D(ImVec2 const& itemMin, ImVec2 const& rectSz, bool& gizmoWantsInput) {
        entt::entity selectedEntity = m_App->SelectedEntity();
        auto& ltrans = m_Ctx->scene.get<Boom::TransformComponent>(selectedEntity);
        glm::mat4 matrix = ltrans.transform.Matrix();

        ImGuizmo::SetOrthographic(true);
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 proj = glm::ortho(-1.f, 1.f, -1.f, 1.f, 0.1f, 1.f);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(itemMin.x, itemMin.y, rectSz.x, rectSz.y);

        ImGuizmo::Manipulate(
            glm::value_ptr(view),          // identity → pure screen space
            glm::value_ptr(proj),
            (ImGuizmo::OPERATION)m_GizmoOperation,
            ImGuizmo::LOCAL,               // always LOCAL for 2D
            glm::value_ptr(matrix),   // or worldMatrix if you want parent space
            nullptr,
            m_UseSnap ? m_SnapValues : nullptr
        );

        gizmoWantsInput = ImGuizmo::IsOver() || ImGuizmo::IsUsing();

        if (ImGuizmo::IsUsing())
        {
            glm::vec3 pos, rot, scale;
            DecomposeTransform(matrix, pos, rot, scale);

            switch (m_GizmoOperation) {
            case ImGuizmo::TRANSLATE:
                ltrans.transform.translate = pos;
                break;
            case ImGuizmo::ROTATE:
                ltrans.transform.rotate = rot;
                break;
            case ImGuizmo::SCALE:
                ltrans.transform.scale = scale;
                break;
            }
        }
    }
    void ViewportPanel::DrawGuizmo3D(
        ImVec2 const& itemMin, ImVec2 const& rectSz, 
        glm::mat4 const& view, glm::mat4 const& proj,
        bool& gizmoWantsInput) 
    {
        // ImGuizmo manipulation
        entt::entity selectedEntity = m_App->SelectedEntity();
        auto& ltrans = m_Ctx->scene.get<Boom::TransformComponent>(selectedEntity);
        glm::mat4 matrix = ltrans.transform.Matrix();

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(itemMin.x, itemMin.y, rectSz.x, rectSz.y);
        ImGuizmo::SetGizmoSizeClipSpace(0.15f);

        const bool viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        if (viewportFocused && !ImGui::GetIO().WantCaptureKeyboard)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_W)) m_GizmoOperation = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed(ImGuiKey_E)) m_GizmoOperation = ImGuizmo::ROTATE;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) m_GizmoOperation = ImGuizmo::SCALE;
            if (ImGui::IsKeyPressed(ImGuiKey_T))
                m_GizmoMode = (m_GizmoMode == ImGuizmo::WORLD) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
        }

        ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());

        ImGuizmo::Manipulate(
            glm::value_ptr(view),
            glm::value_ptr(proj),
            (ImGuizmo::OPERATION)m_GizmoOperation,
            (ImGuizmo::MODE)m_GizmoMode,
            glm::value_ptr(matrix),
            nullptr,
            m_UseSnap ? m_SnapValues : nullptr
        );

        // Check if gizmo wants input
        gizmoWantsInput = ImGuizmo::IsOver() || ImGuizmo::IsUsing();

        if (ImGuizmo::IsUsing())
        {
            glm::vec3 newPosition, newRotation, newScale;
            DecomposeTransform(matrix, newPosition, newRotation, newScale);

            //must be within switch to prevent overwrite
            switch (m_GizmoOperation) {
            case ImGuizmo::TRANSLATE:
                ltrans.transform.translate = newPosition;
                break;
            case ImGuizmo::ROTATE:
                ltrans.transform.rotate = newRotation;
                break;
            case ImGuizmo::SCALE:
                ltrans.transform.scale = newScale;
                break;
            }
        }
        
    }

    void ViewportPanel::HandleMouseClick(const ImVec2& mousePos, const ImVec2&)
    {
        if (!m_Ctx || !m_RayCast) return;

        // Perform ray cast using the current camera data
        entt::entity hitEntity = m_RayCast->CastRayFromScreen(
            mousePos.x, mousePos.y,
            m_CurrentViewMatrix,
            m_CurrentProjectionMatrix,
            m_CurrentCameraPosition,
            m_CurrentViewportSize
        );

        // Update selection
        if (m_App) {
            if (hitEntity != entt::null) {
                m_App->SelectedEntity(true) = hitEntity;

                // Log selection info
                auto& registry = m_Ctx->scene;
                if (registry.all_of<Boom::InfoComponent>(hitEntity)) {
                    const auto& info = registry.get<Boom::InfoComponent>(hitEntity);
                    BOOM_INFO("Selected entity: {} (UID: {})", info.name, info.uid);
                }
                else {
                    BOOM_INFO("Selected entity: {}", static_cast<uint32_t>(hitEntity));
                }
            }
            else {
                // Deselect if clicking empty space
                m_App->SelectedEntity(true) = entt::null;
                BOOM_INFO("Deselected all entities");
            }
        }
    }

    void ViewportPanel::OnSelect(std::uint32_t entity_id)
    {
        DEBUG_DLL_BOUNDARY("ViewportPanel::OnSelect");
        BOOM_INFO("ViewportPanel::OnSelect - Entity selected: {}", entity_id);
    }

    void ViewportPanel::DebugViewportState() const
    {
        BOOM_INFO("=== ViewportPanel Debug State ===");
        BOOM_INFO("Frame ID: {}", m_FrameId);
        BOOM_INFO("Frame Ptr: {}", (void*)m_Frame);
        BOOM_INFO("Viewport Size: {}x{}", m_Viewport.x, m_Viewport.y);

        if (m_FrameId != 0) {
            GLboolean isTexture = glIsTexture(m_FrameId);
            BOOM_INFO("Frame is valid OpenGL texture: {}", isTexture);

            if (isTexture) {
                GLint width, height;
                glBindTexture(GL_TEXTURE_2D, m_FrameId);
                glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
                glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
                BOOM_INFO("Texture actual size: {}x{}", width, height);
                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }
        DebugHelpers::ValidateFrameData(m_FrameId, "ViewportPanel::DebugViewportState");
        BOOM_INFO("=== End Debug State ===");
    }

    std::uint32_t ViewportPanel::QuerySceneFrame() const
    {
        if (m_App) {
            return static_cast<std::uint32_t>(m_App->GetSceneFrame());
        }

        if (m_Ctx && m_Ctx->renderer) {
            return static_cast<std::uint32_t>(m_Ctx->renderer->GetFrame());
        }

        return 0u;
    }

    double ViewportPanel::QueryDeltaTime() const
    {
        if (m_App) return m_App->GetDeltaTime();
        if (m_Ctx) return m_Ctx->DeltaTime;
        return 0.0;
    }

} // namespace EditorUI
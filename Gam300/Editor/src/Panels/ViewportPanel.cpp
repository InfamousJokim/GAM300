// ViewportPanel.cpp - WITH DUAL VIEWPORT SUPPORT (Scene + Game)
#include "Panels/ViewportPanel.h"
#include "Editor.h"
#include "Context/Context.h"
#include "Context/DebugHelpers.h"
#include "Vendors/imgui/imgui.h"
#include "Input/RayCast.h"
#include "Commands/UndoRedo.h"
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

        rotation = glm::degrees(rotation);
    }
}

namespace EditorUI {

    ViewportPanel::~ViewportPanel() = default;

    ViewportPanel::ViewportPanel(Editor* owner)
        : m_Owner(owner)
        , m_IsFullscreen(false)
    {
        m_App = static_cast<Boom::AppInterface*>(m_Owner);
        m_Ctx = m_App ? owner->GetContext() : nullptr;

        if (m_Ctx) {
            m_RayCast = std::make_unique<Boom::RayCast>(m_Ctx);
        }
    }

    void ViewportPanel::Render() { OnShow(); }

    void ViewportPanel::OnShow()
    {
        if (!m_ShowViewport) return;

        auto* app = static_cast<Boom::Application*>(m_Ctx->app);
        if (!app) return;

        // =====================================================
        // DUAL VIEWPORT CONTAINER
        // =====================================================
        ImGui::Begin("Viewports", &m_ShowViewport, ImGuiWindowFlags_NoScrollbar);

        ImVec2 availableSpace = ImGui::GetContentRegionAvail();
        float sceneWidth = availableSpace.x * 0.6f; // 60% for Scene
        float gameWidth = availableSpace.x - sceneWidth - 8.0f; // 40% for Game

        // =====================================================
        // SCENE VIEWPORT (Free Camera)
        // =====================================================
        ImGui::BeginChild("SceneViewport", ImVec2(sceneWidth, availableSpace.y),
            ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);
        {
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "SCENE VIEW");
            ImGui::Separator();

            ImVec2 sceneViewportSize = ImGui::GetContentRegionAvail();

            if (sceneViewportSize.x > 50.0f && sceneViewportSize.y > 50.0f)
            {
                const uint32_t frameTexture = QuerySceneFrame();

                if (frameTexture > 0)
                {
                    // Check if this viewport is focused/hovered
                    bool isHovered = ImGui::IsWindowHovered();
                    bool isFocused = ImGui::IsWindowFocused();

                    // Notify application which viewport is active
                    if (isFocused || isHovered) {
                        app->SetActiveViewport(Boom::ViewportType::SCENE);
                        app->SetSceneViewportFocused(true);
                        app->SetGameViewportFocused(false);
                    }

                    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                    ImGui::GetWindowDrawList()->AddImage(
                        (ImTextureID)(uintptr_t)frameTexture,
                        cursorPos,
                        ImVec2(cursorPos.x + sceneViewportSize.x,
                            cursorPos.y + sceneViewportSize.y),
                        ImVec2(0, 1), ImVec2(1, 0)
                    );

                    ImGui::Dummy(sceneViewportSize);

                    // Camera input region setup
                    const ImVec2 itemMin = ImGui::GetItemRectMin();
                    const ImVec2 itemMax = ImGui::GetItemRectMax();
                    const ImVec2 mainPos = ImGui::GetMainViewport()->Pos;

                    double localX = itemMin.x - mainPos.x;
                    double localY = itemMin.y - mainPos.y;
                    double localW = itemMax.x - itemMin.x;
                    double localH = itemMax.y - itemMin.y;

                    if (m_Ctx && m_Ctx->window) {
                        bool allowInput = isHovered && isFocused;
                        m_Ctx->window->SetCameraInputRegion(localX, localY, localW, localH, allowInput);
                        m_Ctx->window->SetViewportKeyboardFocus(allowInput);
                    }

                    // Tooltip
                    if (isHovered) {
                        ImGui::SetTooltip(
                            "Scene View - Free Camera\n"
                            "Right-Click + Drag: Rotate\n"
                            "WASD: Move | Q/E: Up/Down | Shift: Speed Boost"
                        );
                    }
                }
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // =====================================================
        // GAME VIEWPORT (Main Camera)
        // =====================================================
        ImGui::BeginChild("GameViewport", ImVec2(gameWidth, availableSpace.y),
            ImGuiChildFlags_Border);
        {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "GAME VIEW");
            ImGui::Separator();

            ImVec2 gameViewportSize = ImGui::GetContentRegionAvail();

            if (gameViewportSize.x > 50.0f && gameViewportSize.y > 50.0f)
            {
                const uint32_t frameTexture = QuerySceneFrame();

                if (frameTexture > 0)
                {
                    bool isHovered = ImGui::IsWindowHovered();
                    bool isFocused = ImGui::IsWindowFocused();

                    // Notify application which viewport is active
                    if (isFocused || isHovered) {
                        app->SetActiveViewport(Boom::ViewportType::GAME);
                        app->SetGameViewportFocused(true);
                        app->SetSceneViewportFocused(false);
                    }

                    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                    ImGui::GetWindowDrawList()->AddImage(
                        (ImTextureID)(uintptr_t)frameTexture,
                        cursorPos,
                        ImVec2(cursorPos.x + gameViewportSize.x,
                            cursorPos.y + gameViewportSize.y),
                        ImVec2(0, 1), ImVec2(1, 0)
                    );

                    ImGui::Dummy(gameViewportSize);

                    // Display play mode status
                    ImVec2 textPos = cursorPos;
                    textPos.y += 10;
                    textPos.x += 10;

                    if (app->IsPlaying()) {
                        ImGui::GetWindowDrawList()->AddText(
                            textPos, IM_COL32(0, 255, 0, 255), "PLAYING"
                        );
                    }
                    else {
                        ImGui::GetWindowDrawList()->AddText(
                            textPos, IM_COL32(255, 0, 0, 255), "STOPPED"
                        );
                    }

                    if (isHovered) {
                        ImGui::SetTooltip("Game View - Main Camera\nShows what the player sees");
                    }
                }
            }
        }
        ImGui::EndChild();

        ImGui::End();
    }

    void ViewportPanel::DrawGuizmo2D(ImVec2 const& itemMin, ImVec2 const& rectSz, bool& gizmoWantsInput) {
        entt::entity selectedEntity = m_App->SelectedEntity();
        auto& ltrans = m_Ctx->scene.get<Boom::TransformComponent>(selectedEntity);

        // Use world matrix for gizmo (handles hierarchy correctly, just like 3D gizmo)
        glm::mat4 matrix = Boom::GetWorldMatrix(m_Ctx->scene, selectedEntity);

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
            glm::value_ptr(matrix),        // Use world matrix
            nullptr,
            m_UseSnap ? m_SnapValues : nullptr
        );

        gizmoWantsInput = ImGuizmo::IsUsing();

        // Undo/Redo: Capture transform at start of gizmo manipulation
        if (ImGuizmo::IsUsing() && !m_GizmoWasUsing)
        {
            // Gizmo just started being used - capture initial transform
            if (m_Ctx->scene.all_of<Boom::TransformComponent>(selectedEntity)) {
                m_TransformBeforeGizmo = m_Ctx->scene.get<Boom::TransformComponent>(selectedEntity).transform;
                BOOM_INFO("[Viewport 2D] Captured initial transform for undo");
            }
        }

        if (ImGuizmo::IsUsing())
        {
            // Convert manipulated world matrix back to local space (just like 3D gizmo)
            Boom::SetWorldMatrix(m_Ctx->scene, selectedEntity, matrix);

            // **Sync with RigidBody if present**
            Boom::Entity entity{ &m_Ctx->scene, selectedEntity };
            if (entity.Has<Boom::RigidBodyComponent>())
            {
                m_Owner->GetPhysicsContext().UpdateRigidBodyTransform(entity, ltrans.transform);
            }
        }

        // Undo/Redo: Record command when gizmo is released
        if (!ImGuizmo::IsUsing() && m_GizmoWasUsing)
        {
            // Gizmo was just released - create undo command
            if (m_Ctx->scene.all_of<Boom::TransformComponent>(selectedEntity) && m_Owner)
            {
                auto* history = m_Owner->GetCommandHistory();
                if (history) {
                    const auto& newTransform = m_Ctx->scene.get<Boom::TransformComponent>(selectedEntity).transform;

                    // Only record if transform actually changed
                    bool changed = (m_TransformBeforeGizmo.translate != newTransform.translate) ||
                                  (m_TransformBeforeGizmo.rotate != newTransform.rotate) ||
                                  (m_TransformBeforeGizmo.scale != newTransform.scale);

                    if (changed) {
                        std::string opName;
                        switch (m_GizmoOperation) {
                            case ImGuizmo::TRANSLATE: opName = "Move 2D"; break;
                            case ImGuizmo::ROTATE: opName = "Rotate 2D"; break;
                            case ImGuizmo::SCALE: opName = "Scale 2D"; break;
                            default: opName = "Transform 2D"; break;
                        }

                        std::string entityName = "Entity";
                        if (m_Ctx->scene.all_of<Boom::InfoComponent>(selectedEntity)) {
                            entityName = m_Ctx->scene.get<Boom::InfoComponent>(selectedEntity).name;
                        }

                        auto command = std::make_unique<TransformCommand>(
                            &m_Ctx->scene,
                            selectedEntity,
                            m_TransformBeforeGizmo,
                            newTransform,
                            opName + " '" + entityName + "'"
                        );

                        history->Execute(std::move(command));
                        BOOM_INFO("[Viewport 2D] Recorded transform command for undo");
                    }
                }
            }
        }

        // Update gizmo state (must be outside all blocks to update every frame)
        m_GizmoWasUsing = ImGuizmo::IsUsing();
    }

    void ViewportPanel::DrawGuizmo3D(
        ImVec2 const& itemMin, ImVec2 const& rectSz,
        glm::mat4 const& view, glm::mat4 const& proj,
        bool& gizmoWantsInput)
    {
        // ImGuizmo manipulation
        entt::entity selectedEntity = m_App->SelectedEntity();
        auto& ltrans = m_Ctx->scene.get<Boom::TransformComponent>(selectedEntity);

        // Use world matrix for gizmo (handles hierarchy correctly)
        glm::mat4 matrix = Boom::GetWorldMatrix(m_Ctx->scene, selectedEntity);

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(itemMin.x, itemMin.y, rectSz.x, rectSz.y);
        ImGuizmo::SetGizmoSizeClipSpace(0.15f);

        // Gizmo shortcuts are now handled globally in OnShow(), before this function is called

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
        gizmoWantsInput = ImGuizmo::IsUsing();

        // Undo/Redo: Capture transform at start of gizmo manipulation
        if (ImGuizmo::IsUsing() && !m_GizmoWasUsing)
        {
            // Gizmo just started being used - capture initial transform
            if (m_Ctx->scene.all_of<Boom::TransformComponent>(selectedEntity)) {
                m_TransformBeforeGizmo = m_Ctx->scene.get<Boom::TransformComponent>(selectedEntity).transform;
                BOOM_INFO("[Viewport] Captured initial transform for undo");
            }
        }

        if (ImGuizmo::IsUsing())
        {
            // Convert manipulated world matrix back to local space
            Boom::SetWorldMatrix(m_Ctx->scene, selectedEntity, matrix);
        }

        // Undo/Redo: Record command when gizmo is released
        if (!ImGuizmo::IsUsing() && m_GizmoWasUsing)
        {
            // Gizmo was just released - create undo command
            if (m_Ctx->scene.all_of<Boom::TransformComponent>(selectedEntity) && m_Owner)
            {
                auto* history = m_Owner->GetCommandHistory();
                if (history) {
                    const auto& newTransform = m_Ctx->scene.get<Boom::TransformComponent>(selectedEntity).transform;

                    // Only record if transform actually changed
                    bool changed = (m_TransformBeforeGizmo.translate != newTransform.translate) ||
                                  (m_TransformBeforeGizmo.rotate != newTransform.rotate) ||
                                  (m_TransformBeforeGizmo.scale != newTransform.scale);

                    if (changed) {
                        std::string opName;
                        switch (m_GizmoOperation) {
                            case ImGuizmo::TRANSLATE: opName = "Move"; break;
                            case ImGuizmo::ROTATE: opName = "Rotate"; break;
                            case ImGuizmo::SCALE: opName = "Scale"; break;
                            default: opName = "Transform"; break;
                        }

                        std::string entityName = "Entity";
                        if (m_Ctx->scene.all_of<Boom::InfoComponent>(selectedEntity)) {
                            entityName = m_Ctx->scene.get<Boom::InfoComponent>(selectedEntity).name;
                        }

                        auto command = std::make_unique<TransformCommand>(
                            &m_Ctx->scene,
                            selectedEntity,
                            m_TransformBeforeGizmo,
                            newTransform,
                            opName + " '" + entityName + "'"
                        );

                        history->Execute(std::move(command));
                        BOOM_INFO("[Viewport] Recorded transform command for undo");
                    }
                }
            }

            // **NEW: Sync with RigidBody if present**
            Boom::Entity entity{ &m_Ctx->scene, selectedEntity };
            if (entity.Has<Boom::RigidBodyComponent>())
            {
                m_Owner->GetPhysicsContext().UpdateRigidBodyTransform(entity, ltrans.transform);
            }
        }

        // Update gizmo state (must be outside all blocks to update every frame)
        m_GizmoWasUsing = ImGuizmo::IsUsing();
    }

    void ViewportPanel::HandleMouseClick(uint32_t enttID)
    {
        //must be running context
        if (!m_Ctx) return;
        auto* app = static_cast<Boom::Application*>(m_Ctx->app);
        if (app && app->GetState() == Boom::ApplicationState::RUNNING) {
            return;
        }

        //gather entity from scene
        entt::entity hitEntity{ enttID };

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
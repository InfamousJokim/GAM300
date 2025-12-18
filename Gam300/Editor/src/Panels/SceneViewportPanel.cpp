#include "Panels/SceneViewportPanel.h"
#include "Editor.h"
#include "Context/Context.h"
#include "Input/RayCast.h"
#include "Commands/UndoRedo.h"

namespace EditorUI {

    SceneViewportPanel::SceneViewportPanel(Editor* owner)
        : m_Owner(owner)
    {
        m_App = static_cast<Boom::AppInterface*>(m_Owner);
        m_Ctx = m_App ? owner->GetContext() : nullptr;

        if (m_Ctx) {
            m_RayCast = std::make_unique<Boom::RayCast>(m_Ctx);
        }
    }

    SceneViewportPanel::~SceneViewportPanel() = default;

    void SceneViewportPanel::Render()
    {
        if (!m_ShowViewport) return;

        auto* app = static_cast<Boom::Application*>(m_Ctx->app);
        if (!app) return;

        // =====================================================
        // SCENE VIEWPORT WINDOW (INDEPENDENT)
        // =====================================================
        ImGui::Begin("Scene View", &m_ShowViewport, ImGuiWindowFlags_NoScrollbar);

        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "FREE CAMERA");
        ImGui::Separator();

        ImVec2 viewportSize = ImGui::GetContentRegionAvail();

        if (viewportSize.x > 50.0f && viewportSize.y > 50.0f)
        {
            const uint32_t frameTexture = QuerySceneFrame();

            if (frameTexture > 0)
            {
                bool isHovered = ImGui::IsWindowHovered();
                bool isFocused = ImGui::IsWindowFocused();

                // Notify application this viewport is active
                if (isFocused || isHovered) {
                    app->SetActiveViewport(Boom::ViewportType::SCENE);
                    app->SetSceneViewportFocused(true);
                    app->SetGameViewportFocused(false);
                }

                ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddImage(
                    (ImTextureID)(uintptr_t)frameTexture,
                    cursorPos,
                    ImVec2(cursorPos.x + viewportSize.x,
                        cursorPos.y + viewportSize.y),
                    ImVec2(0, 1), ImVec2(1, 0)
                );

                ImGui::Dummy(viewportSize);

                // Get viewport bounds
                const ImVec2 itemMin = ImGui::GetItemRectMin();
                const ImVec2 itemMax = ImGui::GetItemRectMax();
                const ImVec2 rectSz = ImVec2(itemMax.x - itemMin.x,
                    itemMax.y - itemMin.y);

                // Handle gizmo shortcuts
                bool gizmoShortcutPressed = false;
                if (isFocused && isHovered)
                {
                    if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
                        m_GizmoOperation = ImGuizmo::TRANSLATE;
                        gizmoShortcutPressed = true;
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
                        m_GizmoOperation = ImGuizmo::ROTATE;
                        gizmoShortcutPressed = true;
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
                        m_GizmoOperation = ImGuizmo::SCALE;
                        gizmoShortcutPressed = true;
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_T, false)) {
                        m_GizmoMode = (m_GizmoMode == ImGuizmo::WORLD) ?
                            ImGuizmo::LOCAL : ImGuizmo::WORLD;
                        gizmoShortcutPressed = true;
                    }
                }

                // Gizmo rendering
                bool gizmoWantsInput = false;
                if (m_Ctx)
                {
                    auto camView = m_Ctx->scene.view<Boom::CameraComponent,
                        Boom::TransformComponent>();
                    if (camView.begin() != camView.end())
                    {
                        auto eid = *camView.begin();
                        auto& camComp = camView.get<Boom::CameraComponent>(eid);
                        auto& trans = camView.get<Boom::TransformComponent>(eid);

                        glm::mat4 view = camComp.camera.View(trans.transform);
                        const glm::mat4 proj = camComp.camera.Projection(
                            m_Ctx->renderer->AspectRatio());

                        entt::entity selectedEntity = m_App->SelectedEntity();
                        if (selectedEntity != entt::null &&
                            m_Ctx->scene.valid(selectedEntity))
                        {
                            if (m_Ctx->scene.all_of<Boom::TransformComponent>(
                                selectedEntity))
                            {
                                DrawGuizmo3D(itemMin, rectSz, view, proj,
                                    gizmoWantsInput);
                            }
                        }
                    }
                }

                // Handle mouse clicks for entity selection
                if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                    !gizmoWantsInput)
                {
                    ImVec2 displayedSize = ImGui::GetItemRectSize();
                    ImVec2 displayedPos = ImGui::GetItemRectMin();
                    ImVec2 mousePos = ImGui::GetMousePos();

                    float u = (mousePos.x - displayedPos.x) / displayedSize.x;
                    float v = (mousePos.y - displayedPos.y) / displayedSize.y;

                    if (u >= 0.f && u <= 1.f && v >= 0.f && v <= 1.f) {
                        auto const& fbSize{ m_Ctx->renderer->GetPickSize() };
                        int pickX = (int)(u * fbSize.first);
                        int pickY = (int)(v * fbSize.second);
                        int glY = fbSize.second - pickY - 1;

                        HandleMouseClick(m_Ctx->renderer->GetFrameEnttID(
                            pickX, glY));
                    }
                }

                // Camera input region
                const bool focused = ImGui::IsWindowFocused(
                    ImGuiFocusedFlags_RootAndChildWindows) && isHovered;
                const ImVec2 mainPos = ImGui::GetMainViewport()->Pos;
                const double localX = double(itemMin.x - mainPos.x);
                const double localY = double(itemMin.y - mainPos.y);
                const double localW = double(rectSz.x);
                const double localH = double(rectSz.y);

                if (m_Ctx && m_Ctx->window)
                {
                    const bool allowCameraInput = isHovered && focused &&
                        !gizmoWantsInput;
                    m_Ctx->window->SetCameraInputRegion(localX, localY,
                        localW, localH, allowCameraInput);

                    const bool allowKeyboard = focused && !gizmoWantsInput &&
                        !gizmoShortcutPressed;
                    m_Ctx->window->SetViewportKeyboardFocus(allowKeyboard);
                }

                if (isHovered) {
                    ImGui::SetTooltip(
                        "Scene View - Free Camera\n"
                        "Right-Click + Drag: Rotate\n"
                        "WASD: Move | Q/E: Up/Down | Shift: Speed Boost"
                    );
                }
            }
        }

        ImGui::End();
    }

    std::uint32_t SceneViewportPanel::QuerySceneFrame() const
    {
        if (m_App) {
            return static_cast<std::uint32_t>(m_App->GetSceneFrame());
        }

        if (m_Ctx && m_Ctx->renderer) {
            return static_cast<std::uint32_t>(m_Ctx->renderer->GetFrame());
        }

        return 0u;
    }

    void SceneViewportPanel::HandleMouseClick(uint32_t enttID)
    {
        if (!m_Ctx) return;
        auto* app = static_cast<Boom::Application*>(m_Ctx->app);
        if (app && app->GetState() == Boom::ApplicationState::RUNNING) {
            return;
        }

        entt::entity hitEntity{ enttID };

        if (m_App) {
            if (hitEntity != entt::null) {
                m_App->SelectedEntity(true) = hitEntity;

                auto& registry = m_Ctx->scene;
                if (registry.all_of<Boom::InfoComponent>(hitEntity)) {
                    const auto& info = registry.get<Boom::InfoComponent>(hitEntity);
                    BOOM_INFO("Selected entity: {} (UID: {})", info.name, info.uid);
                }
            }
            else {
                m_App->SelectedEntity(true) = entt::null;
                BOOM_INFO("Deselected all entities");
            }
        }
    }

    void SceneViewportPanel::DrawGuizmo3D(
        ImVec2 const& itemMin, ImVec2 const& rectSz,
        glm::mat4 const& view, glm::mat4 const& proj,
        bool& gizmoWantsInput)
    {
        // Copy your existing DrawGuizmo3D implementation from ViewportPanel.cpp
        // (I'm omitting it here to keep the response concise)
    }

} // namespace EditorUI
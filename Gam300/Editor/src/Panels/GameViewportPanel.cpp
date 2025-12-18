#include "Panels/GameViewportPanel.h"
#include "Editor.h"
#include "Context/Context.h"

namespace EditorUI {

    GameViewportPanel::GameViewportPanel(Editor* owner)
        : m_Owner(owner)
    {
        m_App = static_cast<Boom::AppInterface*>(m_Owner);
        m_Ctx = m_App ? owner->GetContext() : nullptr;
    }

    GameViewportPanel::~GameViewportPanel() = default;

    void GameViewportPanel::Render()
    {
        if (!m_ShowViewport) return;

        auto* app = static_cast<Boom::Application*>(m_Ctx->app);
        if (!app) return;

        // =====================================================
        // GAME VIEWPORT WINDOW (INDEPENDENT)
        // =====================================================
        ImGui::Begin("Game View", &m_ShowViewport, ImGuiWindowFlags_NoScrollbar);

        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "MAIN CAMERA");
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
                    app->SetActiveViewport(Boom::ViewportType::GAME);
                    app->SetGameViewportFocused(true);
                    app->SetSceneViewportFocused(false);
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
                    ImGui::SetTooltip(
                        "Game View - Main Camera\n"
                        "Shows what the player sees\n"
                        "(Read-Only)"
                    );
                }
            }
        }

        ImGui::End();
    }

    std::uint32_t GameViewportPanel::QuerySceneFrame() const
    {
        if (m_App) {
            return static_cast<std::uint32_t>(m_App->GetSceneFrame());
        }

        if (m_Ctx && m_Ctx->renderer) {
            return static_cast<std::uint32_t>(m_Ctx->renderer->GetFrame());
        }

        return 0u;
    }

} // namespace EditorUI
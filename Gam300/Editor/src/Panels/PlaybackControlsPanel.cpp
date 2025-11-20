#include "Panels/PlaybackControlsPanel.h"
#include "Editor.h"
#include "Context/Context.h"
#include "Context/DebugHelpers.h"
#include "AppWindow.h"           // declares Boom::Application / ApplicationState
#include "Vendors/imgui/imgui.h"

namespace EditorUI {

    PlaybackControlsPanel::PlaybackControlsPanel(Editor* owner, Boom::Application* app)
        : m_Owner(owner), m_App(app)
    {
        DEBUG_DLL_BOUNDARY("PlaybackControlsPanel::Ctor");
        if (!m_Owner) { BOOM_ERROR("PlaybackControls - null owner"); return; }
        m_Ctx = m_Owner->GetContext();
        DEBUG_POINTER(m_Ctx, "AppContext");
    }

    void PlaybackControlsPanel::Render() { OnShow(); }

    void PlaybackControlsPanel::OnShow()
    {
        if (!m_Show) return;

        if (ImGui::Begin("Playback Controls", &m_Show))
        {
            auto* app = m_App;
            if (!app) { ImGui::TextDisabled("Application not wired"); ImGui::End(); return; }

            auto state = app->GetState();

            ImGui::Text("Application State: "); ImGui::SameLine();
            switch (state) {
            case Boom::ApplicationState::RUNNING: ImGui::TextColored(ImVec4(0, 1, 0, 1), "RUNNING"); break;
            case Boom::ApplicationState::PAUSED:  ImGui::TextColored(ImVec4(1, 1, 0, 1), "PAUSED");  break;
            case Boom::ApplicationState::STOPPED: ImGui::TextColored(ImVec4(1, 0, 0, 1), "STOPPED"); break;
            }

            ImGui::Separator();
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));

            // Play/Resume
            {
                bool can = (state == Boom::ApplicationState::PAUSED || state == Boom::ApplicationState::STOPPED);
                if (!can) ImGui::BeginDisabled();
                if (ImGui::Button("Play/Resume", ImVec2(100, 30)) && can) app->Resume();
                if (!can) ImGui::EndDisabled();
            }

            ImGui::SameLine();

            // Pause
            {
                bool can = (state == Boom::ApplicationState::RUNNING);
                if (!can) ImGui::BeginDisabled();
                if (ImGui::Button("Pause", ImVec2(100, 30)) && can) app->Pause();
                if (!can) ImGui::EndDisabled();
            }

            ImGui::SameLine();

            // Stop
            {
                bool can = (state != Boom::ApplicationState::STOPPED);
                if (!can) ImGui::BeginDisabled();
                if (ImGui::Button("Stop", ImVec2(100, 30)) && can) app->Stop();
                if (!can) ImGui::EndDisabled();
            }

            ImGui::PopStyleVar();

            ImGui::Separator();
            ImGui::Text("Keyboard Shortcuts:");
            ImGui::BulletText("Spacebar: Toggle Pause/Resume");
            ImGui::BulletText("Escape: Stop Application");

            if (state != Boom::ApplicationState::STOPPED) {
                ImGui::Separator();
                ImGui::Text("Adjusted Time: %.2f seconds", app->GetAdjustedTime());
                if (state == Boom::ApplicationState::PAUSED)
                    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Time is paused");
            }
        }
        ImGui::End();
    }

} // namespace EditorUI
